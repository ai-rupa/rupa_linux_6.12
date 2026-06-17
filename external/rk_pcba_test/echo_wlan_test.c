/*
 *  wlan_test.c  --  wlan test application
 *
 *  Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *  Author: Panzhenzhuan Wang <randy.wang@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// WLAN test program
#include <stdio.h>
// Header files required for the open function
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// error related header files
#include <errno.h>
#include <string.h>
#include "wlan_test.h"

#define LOG_TAG "wlan_test"
#include "common.h"
#define WLAN_PROC_ERR -45

#define WLAN_START_UP_COMMAND "ifconfig wlan0 up"
#define WLAN_MANAGE_START "wpa_supplicant -i wlan0 -c /etc/wpa_supplicant.conf &"
#define WLAN_SCAN_COMMAND "wpa_cli -i wlan0 scan 'wlan0'"
#define WLAN_SCAN_RESULT  "wpa_cli -i wlan0 scan_r | busybox awk 'NR>=3{print $5,$3}'"
#define SCAN_RESULT_FILE "/tmp/wlan_scan_result.txt"

#define AP_SUPPORT_CMD "iw list | busybox grep AP > %s"
#define AP_SUPPORT_FILE "/tmp/wlan_ap_result.txt"
#define SOFTAP_MSG "dmesg | grep SSID | tail -n 6 > %s"
#define MSG_FILE "/tmp/softap_start_msg.txt"

#define CONNECT_AP_CMD "wpa_supplicant -i wlan0 -c /data/wpa_conf &"
#define CONNECT_AP_STATUS "wpa_cli status > %s"
#define CONNECT_AP_RESULT_FILE "/tmp/wlan_connect_result.txt"
#define REPORT_SSID     "Rockchip" // Specify the Wi-Fi SSID name to scan

// 1. Terminate the wpa_supplicant background process.
static int close_wpa_supplicant(void)
{
    int test_flag = -1;
    char pid_buf[64];
    char cmd[64];
    FILE* pp;
    printf("====================function : %s start =================\n",__func__);
    pp = popen("ps aux| grep wpa_supplicant | awk 'NR==1 {print $2}'","r");
    if (!pp) {
        printf("%s popen err%s\n",__func__,strerror(errno));
        return -1;
    }
    memset(pid_buf,0,sizeof(pid_buf));
    if (fgets(pid_buf,sizeof(pid_buf),pp) != NULL) {
        pid_buf[strcspn(pid_buf, "\n")] = 0;
        if (strlen(pid_buf) > 0) {
            printf("Get pid_buf is: \t %s\n",pid_buf);
            sprintf(cmd,"kill -9 %s", pid_buf);
            printf("cmd is: %s\n",cmd);
            system(cmd);
        } else {
            printf("No wpa_supplicant process found\n");
        }
    }
    pclose(pp);
    printf("====================function : %s finish =================\n", __func__);
    return 0;
}

/*
 * RSSI Levels as used by notification icon
 *
 * Level 4  -55 <= RSSI
 * Level 3  -66 <= RSSI < -55
 * Level 2  -77 <= RSSI < -67
 * Level 1  -88 <= RSSI < -78
 * Level 0         RSSI < -88
 */
static int calc_rssi_lvl(int rssi)
{
    if (rssi >= -55)
        return 4;
    else if (rssi >= -66)
        return 3;
    else if (rssi >= -77)
        return 2;
    else if (rssi >= -88)
        return 1;
    else
        return 0;
}

/* Output scanned WiFi information to the file
specified by the path variable and display
the first WiFi name and signal strength. */
int wlan_exec(const char *cmd, char *path)
{
    char result_buf[128];
    char ssid[128];
    int signal_level;
    int ch = 0;
    int fd;
    int ret = 0;
    chip_id_t chip_id = get_chipid();
    printf("=================== function :%s start======================\n\n",__func__);
    memset(result_buf, 0, sizeof(result_buf));
    memset(ssid, 0, sizeof(ssid));
    // Display the first WiFi information
    FILE *pp = popen(cmd, "r");
    // If the file fails to open, an error message will be displayed.
    if (!pp) {
        printf("%s popen err%s\n", __func__, strerror(errno));
        return -1;
    }
    switch (chip_id) {
        case CHIP_PX3SE:
            /*chad.ma add below*/
            if (path != NULL) {
                fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IROTH);
                if (fd <0) {
                    log_err("open %s fail, errno = %d\n", path, errno);
                    pclose(pp);
                    return errno;
                }
            }
            /*chad.ma add up*/
            break;
        default:
            break;
    }
    int scan_count = 0;
    while(!feof(pp)) {
        fscanf(pp,"%s",ssid);
        fscanf(pp,"%d",&signal_level);
        scan_count++;
        // Determine whether the WiFi is functioning properly based on signal strength.
        if (signal_level< -200 || signal_level >= 0) {
            printf("wlan scan result is null\n");
            ret = -1;
            continue;
        } else {
            ret = 0;
        }
        switch (chip_id) {
            case CHIP_PX3SE:
                snprintf(result_buf,sizeof(result_buf),
                        "SSID:%s Signal Level:%d RSSI:%d\n",
                        ssid, signal_level, calc_rssi_lvl(signal_level));
                int w_len = write(fd, result_buf, strlen(result_buf));
                if (w_len <= 0) {
                    log_err("Write %s fail, errno = %d\n", path, errno);
                    ret = errno;
                }
                break;
            default:
                break;
        }
        printf("SSID is: %s,signal level is: %d\n",ssid,signal_level);
        printf("SSID is: %s,rssi is: %d\n",ssid,calc_rssi_lvl(signal_level));
        if (scan_count == 0) {
            ret = -1;
        }
    }
    pclose(pp);
    switch (chip_id) {
        case CHIP_PX3SE:
            close(fd);
            break;
        default:
            break;
    }
    printf("\n=================== function :%s finish======================\n", __func__);
    return ret;
}

void *wlan_test(void *argv)
{
    int test_flag=-1;
    char cmd[128];
    printf("===================Wlan test start======================\n");
    //sprintf(cmd,"aplay %s/wlan_test_start.wav",AUDIO_PATH);
    //system(cmd);
    //system("aplay /data/cfg/test/wlan_test_start.wav");

    /* 1. First, terminate the wpa_supplicant background process.
    Then unload the WLAN driver firmware, followed by reloading the driver firmware. */
    test_flag = close_wpa_supplicant();
    system("echo 1 > /sys/class/rkwifi/driver");
    /* 2. Enable the wlan0 interface. */
    int status = system(WLAN_START_UP_COMMAND);
    /* add by chad ma 20180629 below judge*/
    if(status == -1) {
        printf("system run error ....\n");
        goto fail;
    } else {
        log_info("exit status value = [0x%x]\n", status);
        if (WIFEXITED(status)) {
            if (0 == WEXITSTATUS(status)) {
                log_info("run WLAN_START_UP_COMMAND successfully.\n");
                //ret = 0;
            } else {
                system("rfkill unblock wifi");
            }
        } else {
            log_info("exit status = [%d]\n", WEXITSTATUS(status));
        }
    }
    sleep(2);
    /* 3. Start the wireless network adapter management program wpa_supplicant */
    system(WLAN_MANAGE_START);
    sleep(2);
    /* 4. Scan for wireless networks */
    system(WLAN_SCAN_COMMAND);
    sleep(3);
    /* 5. Display WiFi information */
    test_flag = wlan_exec(WLAN_SCAN_RESULT,SCAN_RESULT_FILE);
    if(test_flag < 0) {
        printf("First scan result failed, retrying...\n");
        sleep(2);
        // The first scan failed; please run it again.
        system(WLAN_SCAN_COMMAND);
        sleep(2);
        test_flag = wlan_exec(WLAN_SCAN_RESULT,SCAN_RESULT_FILE);
        if(test_flag<0)
        goto fail;
    }
    printf("===================Wlan test success ======================\n");
    system("ifconfig wlan0 down");
    system("busybox killall wpa_supplicant"); // Disable the wireless management program wpa_supplicant
    return (void*)test_flag;
fail:
    printf("===================Wlan test failed======================\n");
    system("ifconfig wlan0 down");
    system("busybox killall wpa_supplicant"); // Disable the wireless management program wpa_supplicant
    return (void*)test_flag;
}

int main(int argc, char *argv[])
{
    int test_flag = 0,err_code = 0;
    char read_buf[128];
    int find_flag = 0;
    FILE *fp;
    chip_id_t chip_id = get_chipid();
    char buf[COMMAND_VALUESIZE] = "wlan_test";
    char result[COMMAND_VALUESIZE] = RESULT_PASS;
    test_flag = (int)wlan_test(argv[0]);
    if (test_flag < 0) {
        strcpy(result,RESULT_FAIL);
        err_code = WLAN_PROC_ERR;
    }
    switch (chip_id) {
        case CHIP_PX3SE:
            // From the saved scan results file, scan the signal for the specified SSID.
            fp = fopen(SCAN_RESULT_FILE, "r");
            memset(read_buf, 0 ,sizeof(read_buf));
            // If the file fails to open, an error message will be displayed.
            if (!fp) {
                printf("%s fopen err:%s\n",__func__,strerror(errno));
                return -1;
            }
            while(!feof(fp)) {
                fgets(read_buf,sizeof(read_buf),fp);
                if(strstr(read_buf,REPORT_SSID)!= NULL) {
                    find_flag = 1;
                    /* Note: read_buf will also read the carriage
                    return character at the end of a line.
                    Modifications to read_buf are required here. */
                    int len = strlen(read_buf);
                    if ( len < 128) {
                        //assert(len < 128);
                        read_buf[len - 1] = '\0';
                        printf("\n ###Found!!! %s ###\n",read_buf);
                        break;
                    } else {
                        printf("%s ERROR!!! read to much from one line\n",__func__);
                    }
                }
            }
            fclose(fp);
            // Copy the saved result to buf.
            if (find_flag) {
                strcat(buf, ": ");
                strcat(buf, read_buf);
            }
            // Delete scanned files
            // remove(SCAN_RESULT_FILE);
            printf("\n $$ %s :not found %s $$\n",buf , REPORT_SSID);
            break;
        default:
            break;
    }
    send_msg_to_server(buf, result, err_code);
}
