// this is the normal build target ESP include set
#include "espmissingincludes.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "upgrade.h"

#include "config_cmd.h"


#define MSG_OK "OK\r\n"
#define MSG_ERROR "ERROR\r\n"
#define MSG_INVALID_CMD "UNKNOWN COMMAND\r\n"

#define MAX_ARGS 4
#define MSG_BUF_LEN 128

bool doflash = true;

static void ICACHE_FLASH_ATTR handleUpgrade(uint8_t serverVersion, const char *server_ip, uint16_t port, const char *path);

char *my_strdup(char *str) {
    size_t len;
    char *copy;

    len = strlen(str) + 1;
    if (!(copy = (char*)os_malloc((u_int)len)))
        return (NULL);
    os_memcpy(copy, str, len);
    return (copy);
}

char **config_parse_args(char *buf, uint8_t *argc) {
    const char delim[] = " \t";
    char *save, *tok;
    char **argv = (char **)os_malloc(sizeof(char *) * MAX_ARGS);    // note fixed length
    os_memset(argv, 0, sizeof(char *) * MAX_ARGS);

    *argc = 0;
    for (; *buf == ' ' || *buf == '\t'; ++buf); // absorb leading spaces
    for (tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
        argv[*argc] = my_strdup(tok);
        (*argc)++;
        if (*argc == MAX_ARGS) {
            break;
        }
    }
    return argv;
}

void config_parse_args_free(uint8_t argc, char *argv[]) {
    uint8_t i;
    for (i = 0; i <= argc; ++i) {
        if (argv[i])
            os_free(argv[i]);
    }
    os_free(argv);
}

void config_cmd_flash(serverConnData *conn, uint8_t argc, char *argv[]) {
    bool err = false;
    if (argc == 0)
        espbuffsentprintf(conn, "FLASH=%d\r\n", doflash);
    else if (argc != 1)
        err=true;
    else {
        if (strcmp(argv[1], "1") == 0)
            doflash = true;
        else if (strcmp(argv[1], "0") == 0)
            doflash = false;
        else
            err=true;
    }
    if (err)
        espbuffsentstring(conn, MSG_ERROR);
    else
        espbuffsentstring(conn, MSG_OK);
}

void config_restart(serverConnData *conn, uint8_t argc, char *argv[]) {
    system_restart();
}

#ifdef OTAENABLED
void ota_upgrade(serverConnData *conn, uint8_t argc, char *argv[]) {
    espbuffsentprintf(conn, "START OTA UPGRADE...CUR ROM: %d.\r\n", ROMNUM);

    if (argc == 0)
        espbuffsentstring(conn, "OTA <SRV IP> <PORT> <FOLDER PATH>\r\n");
    else if (argc != 3)
        espbuffsentstring(conn, MSG_ERROR);
    else {
        uint16_t port = atoi(argv[2]);
        uint8_t iparr[4];
        uint8_t i;
        char tmp[4];
        char* pCurIp = argv[1];
        char * ch;

        os_printf("\r\nOTA UPGRADE with IP:%s, port:%d, path:%s", argv[1], argv[2], argv[3]);
        ch = strchr(pCurIp, '.');
#if DEBUG
        os_printf("\r\nFirst: %s", pCurIp);
        os_printf("\r\nAfter: %s", ch);
#endif
        for (i = 0; i < 4; ++i) {
            if (i < (ch - pCurIp + 1)) {
                tmp[i] = pCurIp[i];
            } else {
                tmp[i] = 0;
            }
        }
        iparr[0] = atoi(tmp);
        pCurIp = ch + 1;

        ch = strchr(pCurIp, '.');
        os_printf("\r\nFirst: %s", pCurIp);
        os_printf("\r\nAfter: %s", ch);
        for (i = 0; i < 4; ++i) {
            if (i < (ch - pCurIp + 1)) {
                tmp[i] = pCurIp[i];
            } else {
                tmp[i] = 0;
            }
        }
        iparr[1] = atoi(tmp);
        pCurIp = ch + 1;

        ch = strchr(pCurIp, '.');
        os_printf("\r\nFirst: %s", pCurIp);
        os_printf("\r\nAfter: %s", ch);
        for (i = 0; i < 4; ++i) {
            if (i < (ch - pCurIp + 1)) {
                tmp[i] = pCurIp[i];
            } else {
                tmp[i] = 0;
            }
        }
        iparr[2] = atoi(tmp);
        pCurIp = ch + 1;

        ch = strchr(pCurIp, '.');
        os_printf("\r\nFirst: %s", pCurIp);
        os_printf("\r\nAfter: %s", ch);
        for (i = 0; i < 4; ++i) {
            if (i < (argv[1] + strlen(argv[1]) - pCurIp + 1)) {
                tmp[i] = pCurIp[i];
            } else {
                tmp[i] = 0;
            }
        }
        iparr[3] = atoi(tmp);

        os_printf("\r\n=> IP: %d %d %d %d", iparr[0], iparr[1], iparr[2], iparr[3]);

        if ((port == 0)||(port>65535)) {
             espbuffsentstring(conn, MSG_ERROR);
        } else {
            espbuffsentstring(conn, MSG_OK);
            handleUpgrade(2, iparr, port, argv[3]);
        }
    }
    // debug
    // {
    //     espbuffsentprintf(conn, "flash param:\n\tmagic\t%d\n\tversion\t%d\n\tbaud\t%d\n\tport\t%d\n",
    //         flash_param->magic, flash_param->version, flash_param->baud, flash_param->port);
    // }
}
#endif

const config_commands_t config_commands[] = {
#ifdef OTAENABLED
        { "OTA", &ota_upgrade },
#endif
        { "FLASH", &config_cmd_flash },
        { "RST", &config_restart },
        { NULL, NULL }
    };

void config_parse(serverConnData *conn, char *buf, int len) {
    char *lbuf = (char *)os_malloc(len + 1), **argv;
    uint8_t i, argc;
    // we need a '\0' end of the string
    os_memcpy(lbuf, buf, len);
    lbuf[len] = '\0';

    // command echo
    // espbuffsent(conn, lbuf, len);

    // remove any CR / LF
    for (i = 0; i < len; ++i)
        if (lbuf[i] == '\n' || lbuf[i] == '\r')
            lbuf[i] = '\0';

    // verify the command prefix
    if (os_strncmp(lbuf, "+++AT", 5) != 0) {
        return;
    }
    // parse out buffer into arguments
    argv = config_parse_args(&lbuf[5], &argc);
#if 0
// debugging
    {
        uint8_t i;
        for (i = 0; i < argc; ++i) {
            espbuffsentprintf(conn, "argument %d: '%s'\r\n", i, argv[i]);
        }
    }
// end debugging
#endif
    if (argc == 0) {
        espbuffsentstring(conn, MSG_OK);
    } else {
        argc--; // to mimic C main() argc argv
        for (i = 0; config_commands[i].command; ++i) {
            if (os_strncmp(argv[0], config_commands[i].command, strlen(argv[0])) == 0) {
                config_commands[i].function(conn, argc, argv);
                break;
            }
        }
        if (!config_commands[i].command)
            espbuffsentprintf(conn, "%s - buf: %s\r\n", MSG_INVALID_CMD, argv[0]);
    }
    config_parse_args_free(argc, argv);
    os_free(lbuf);
}

#ifdef OTAENABLED
static void ICACHE_FLASH_ATTR ota_finished_callback(void *arg)
{
    struct upgrade_server_info *update = arg;
    if (update->upgrade_flag == true)
    {
        os_printf("[OTA]success; rebooting!\n");
        system_upgrade_reboot();
    }
    else
    {
        os_printf("[OTA]failed! %d - %d\n", update->upgrade_flag, system_upgrade_flag_check());
    }

    os_free(update->pespconn);
    os_free(update->url);
    os_free(update);
}
 
static void ICACHE_FLASH_ATTR handleUpgrade(uint8_t serverVersion, const char *server_ip, uint16_t port, const char *path)
{
    const char* file;
    uint8_t userBin = system_upgrade_userbin_check();
    os_printf("\r\nUserBIn = %d", userBin);
    switch (ROMNUM)
    {
        case 1: file = "user2.1024.new.2.bin"; break;
        case 2: file = "user1.1024.new.2.bin"; break;
        default:
            os_printf("[OTA]Invalid userbin number!\n");
            return;
    }

    uint16_t version=1;
    if (serverVersion <= version)
    {
        os_printf("[OTA]No update. Server version:%d, local version %d\n", serverVersion, version);
        return;
    }

    os_printf("[OTA]Upgrade available version: %d\n", serverVersion);

    struct upgrade_server_info* update = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));
    update->pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));

    os_memcpy(update->ip, server_ip, 4);
    update->port = port;

    os_printf("[OTA]Server "IPSTR":%d. Path: %s%s\n", IP2STR(update->ip), update->port, path, file);

    update->check_cb = ota_finished_callback;
    update->check_times = 10000;
    update->url = (uint8 *)os_zalloc(512);

    os_sprintf((char*)update->url,
    "GET %s%s HTTP/1.1\r\n"
    "Host: "IPSTR":%d\r\n"
    "Connection: close\r\n"
    "\r\n",
    path, file, IP2STR(update->ip), update->port);
    os_printf("\r\nUpdate url: %s", update->url);

    if (system_upgrade_start(update) == false)
    {
        os_printf("[OTA]Could not start upgrade\n");

        os_free(update->pespconn);
        os_free(update->url);
        os_free(update);
    }
    else
    {
        os_printf("[OTA]Upgrading...\n");
    }
}
#endif