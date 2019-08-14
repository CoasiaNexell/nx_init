#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <dirent.h>
#include <fcntl.h>

enum {
	CAM_TYPE_QUICKREAR	= 0x0,
	CAM_TYPE_1CAMTOPVIEW	= 0x1,
	CAM_TYPE_4CAMSVM	= 0x2
};

#define CMDLINE_READ_SIZE 2048
#define OPTION_CNT_MAX 16

struct nx_cam_option {
	int option;
	char buffer[32];
};

struct option_string {
	char string[32];
};

const char *rearcam_type = "nx_rearcam=";
const char *product_partnum= "product_part=";

static struct option_string string[] = {
	{"nx_cam.m="},
	{"nx_cam.g="},
	{"nx_cam.b="},
	{"nx_cam.c="},
	{"nx_cam.v="},
	{"nx_cam.p="},
	{"nx_cam.r="},
	{"nx_cam.d="},
	{"nx_cam.l="},
	{"nx_cam.s="},
	{"nx_cam.D="},
	{"nx_cam.R="},
	{"nx_cam.P="},
	{"nx_cam.L="},
	{"nx_cam.end"},
};

static int search(const char *str_opt, const char *src, char *dest)
{
	const char *pos, *pos1;
	int len;
	int opt_len;

	if (strncmp(str_opt, "nx_cam.end", strlen("nx_cam.end")) == 0)
		return -1;

	pos = strstr(src, str_opt);
	if (pos) {
		len = strlen(str_opt);
		pos += len;

		pos1 = strstr(pos, " ");
		opt_len = pos1 - pos;

		strncpy(dest, pos, opt_len);

		return 0;
	}

	return -1;
}

static int search_option(const char *src, struct nx_cam_option *option)
{
	int i;
	int string_cnt = sizeof(string)/sizeof(struct option_string);
	int opt_cnt = 0;
	char tmp_buffer[32];

	if (string_cnt > OPTION_CNT_MAX) {
		string_cnt = OPTION_CNT_MAX;
		printf("check the MAX count of option:%d\n", OPTION_CNT_MAX);
	}

	if (search("nx_cam.", src, tmp_buffer) != 0)
		return opt_cnt;

	for (i = 0; i < string_cnt; i++) {
		if (search(string[i].string, src, option->buffer) == 0) {
			/* printf("option->buffer:%s\n", option->buffer); */
			option->option = 1;
			option++;
			opt_cnt++;
		}
	}

	return opt_cnt;
}

static int runNxQuickRearCam(int cnt, struct nx_cam_option *option)
{
	if (cnt == 0) {
		printf("start NxQuickRearCam (default)\n");
		execl("/sbin/NxQuickRearCam", "NxQuickRearCam", "-m1", "-b1"
		      , "-c26", "-r704x480", NULL);
	} else {
		printf("start NxQuickRearCam %s %s %s %s %s %s %s %s %s %s %s "
		       "%s %s %s %s %s\n"
		       , option[0].buffer, option[1].buffer, option[2].buffer
		       , option[3].buffer, option[4].buffer, option[5].buffer
		       , option[6].buffer, option[7].buffer, option[8].buffer
		       , option[9].buffer, option[10].buffer, option[11].buffer
		       , option[12].buffer, option[13].buffer, option[14].buffer
		       , option[15].buffer, NULL);
		execl("/sbin/NxQuickRearCam", "NxQuickRearCam"
		      , option[0].buffer, option[1].buffer, option[2].buffer
		      , option[3].buffer, option[4].buffer, option[5].buffer
		      , option[6].buffer, option[7].buffer, option[8].buffer
		      , option[9].buffer, option[10].buffer, option[11].buffer
		      , option[12].buffer, option[13].buffer, option[14].buffer
		      , option[15].buffer, NULL);
	}
	return 0;

}

int main(int argc, char *argv[])
{
	pid_t pid;
	int status = 0;
	int access_ret = 0;
	int opt_cnt = 0;
	long nxrear_cam = 0;

	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("proc", "/proc", "proc", 0, NULL);

	pid = fork();

	switch(pid)
	{
		case -1:
		{
			printf("fail create child process \n");
			return -1;
		}

		case 0:
		{
			FILE *fp;
			int r_size = 0;
			char cmdline[CMDLINE_READ_SIZE];
			char *ptr;
			char product_blkname[32];

			fp = fopen("/proc/cmdline", "r");
			if (fp == NULL) {
				printf("/proc/cmdline read failed \n");
				return -1;
			}

			r_size = fread(cmdline, 1, CMDLINE_READ_SIZE, fp);
			if (r_size == CMDLINE_READ_SIZE) {
				printf("check size of cmdline!!!\n");
				return -1;
			}
			fclose(fp);

			/* check rear camera type */
			ptr = strstr(cmdline, rearcam_type);
			if (ptr) {
				int len = strlen(rearcam_type);
				nxrear_cam = strtol(ptr+len, NULL, 10);
			} else {
				/* does not define nx_rearcam */
				nxrear_cam = CAM_TYPE_QUICKREAR;
			}
			/* find product partition number */
			ptr = strstr(cmdline, product_partnum);
			if (ptr) {
				ptr += strlen(product_partnum);
				sscanf(ptr, "%s ",product_blkname);
				mount(product_blkname,"/product", "ext4", 0, NULL);
			} else {
				/* does not define product */
				printf("cmdline do not have %s \n", product_partnum);
			}

			if (nxrear_cam == CAM_TYPE_4CAMSVM
			    || nxrear_cam == CAM_TYPE_1CAMTOPVIEW) {

				if (nxrear_cam == CAM_TYPE_4CAMSVM) {
					printf("start nx_3d_avm_daemon 4\n");
					execl("/sbin/nx_3d_avm_daemon"
					      , "nx_3d_avm_daemon", "4", NULL);
				}
				else {
					printf("start nx_3d_avm_daemon 1\n");
					execl("/sbin/nx_3d_avm_daemon"
					      , "nx_3d_avm_daemon", "1", NULL);
				}
			} else {	/* CAM_TYPE_QUICKREAR */
				struct nx_cam_option cam_option[OPTION_CNT_MAX];

				access_ret = access("/sbin/NxQuickRearCam", 0);
				if (access_ret == 0) {
					int size = 0;
					size = sizeof(struct nx_cam_option)
						* OPTION_CNT_MAX;
					memset((void*)cam_option, 0x0, size);
					opt_cnt = 0;
					opt_cnt = search_option(cmdline , cam_option);

					runNxQuickRearCam(opt_cnt, cam_option);
				}
				else {
					printf("/sbin/NxQuickRearCam is not exist \n");
				}

			}
			break;
		}

		default:
		{
			#ifndef ANDROID
			usleep(500000);
			printf("start systemd \n");
			execl("/lib/systemd/systemd","systemd", NULL);
			#else
			printf("start android init \n");
			execl("/init","init", NULL);
			#endif
			break;
		}
	}

	waitpid(pid, &status, 0);

	return 0;
}
