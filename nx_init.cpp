#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <string.h>

enum {
	CAM_TYPE_QUICKREAR	= 0x0,
	CAM_TYPE_1CAMTOPVIEW	= 0x1,
	CAM_TYPE_4CAMSVM	= 0x2
};


#define CMDLINE_READ_SIZE 512
#define OPTION_CNT_MAX 16

struct nx_cam_option {
	int option;
	char buffer[32];
};

struct option_string {
	char string[32];
};

char *rearcam_type = "nx_rearcam=";

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
		       , option[15].buffer);
		execl("/sbin/NxQuickRearCam", "NxQuickRearCam"
		      , option[0].buffer, option[1].buffer, option[2].buffer
		      , option[3].buffer, option[4].buffer, option[5].buffer
		      , option[6].buffer, option[7].buffer, option[8].buffer
		      , option[9].buffer, option[10].buffer, option[11].buffer
		      , option[12].buffer, option[13].buffer, option[14].buffer
		      , option[15].buffer);
	}
	return 0;

}

int main(int argc, char *argv[])
{
	pid_t pid;
	int status = 0;
	int access_ret = 0;
	int fd = 0;
	int r_size = 0;
	int opt_cnt = 0;
	char cmdline[CMDLINE_READ_SIZE];
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

		case 0: {
			fd = open("/proc/cmdline", O_RDONLY);
			if (fd >= 0) {
				r_size = read(fd, cmdline, CMDLINE_READ_SIZE);
				if (r_size == CMDLINE_READ_SIZE)
					printf("check size of cmdline!!!\n");

				// check rear camera type
				char *ptr = strstr(cmdline, rearcam_type);
				if (ptr) {
					int len = strlen(rearcam_type);
					nxrear_cam = strtol(ptr+len, NULL, 10);
				} else {
					/* does not define nx_rearcam */
					nxrear_cam = CAM_TYPE_QUICKREAR;
				}
			} else {
				/* set default NxQuickRearCam */
				nxrear_cam = CAM_TYPE_QUICKREAR;
			}

			if (nxrear_cam == CAM_TYPE_4CAMSVM
			    || nxrear_cam == CAM_TYPE_1CAMTOPVIEW) {
				if(fd)
					close(fd);

				mkdir("/svmdata", 0755);
				mount("/dev/mmcblk0p2", "/svmdata", "ext4", 0
				      , NULL);

				if (nxrear_cam == CAM_TYPE_4CAMSVM) {
					printf("start nx_3d_avm_daemon 4\n");
					execl("/sbin/nx_3d_avm_daemon"
					      , "nx_3d_avm_daemon", "4", NULL);
				} else {
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

					if (fd >= 0) {
						opt_cnt = search_option(cmdline
								, cam_option);
						close(fd);
					} else {
						printf("does not found "
						       "NxQuickRearCam option.\n");
					}

					runNxQuickRearCam(opt_cnt, cam_option);
				}
			}
			break;
		}

		default:
		{
			usleep(500000);
			printf("start systemd \n");
			execl("/lib/systemd/systemd","systemd", NULL);
			break;
		}
	}

	waitpid(pid, &status, 0);

	return 0;
}
