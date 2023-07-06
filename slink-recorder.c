#define DR_WAV_IMPLEMENTATION
#include "./dr_wav.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <linux/if_ether.h>

int kbhit()
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

void printBufferAsHex(const unsigned char *buffer, size_t length)
{
	for (size_t i = 0; i < length; i++)
	{
		printf("%02X ", buffer[i]);
	}
	printf("\n");
}

uint32_t switchByteOrder24(uint32_t src)
{
	// Switch byte 0 and 2
	src = (src & 0xff0000) >> 16 | (src & 0x00ff00) | (src & 0x0000ff) << 16;
	// Switch nibble 0 and 1 in each byte
	src = (src & 0xf0f0f0) >> 4 | (src & 0x0f0f0f) << 4;
	return src;
}

void printBufferAtByte14(const unsigned char *buffer)
{
	uint8_t value = buffer[13] + buffer[14];
	printf("Value at byte 14: %u\n", value);
}

int main(int argc, char **argv)
{
	drwav_data_format format;
	drwav wav;
	int8_t tempFrames[4096][1][3];
	if (argc < 3)
	{
		printf("Usage: %s <output_file> <ace_channel>\n", argv[0]);
		return -1;
	}

	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = 1;
	format.sampleRate = 96000;
	format.bitsPerSample = 24;
	if (!drwav_init_file_write(&wav, argv[1], &format, NULL))
	{
		printf("Failed to open file.\n");
		return -1;
	}

	int sock_r, saddr_len;

	unsigned char *buffer = (unsigned char *)malloc(65536);
	memset(buffer, 0, 65536);

	sock_r = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_r < 0)
	{
		printf("Error in socket, run as super user.\n");
		return -1;
	}

	int i = 0;
	int ace_channel = atoi(argv[2]);
	printf("Recording, press ENTER to stop .... \n");

	while (!kbhit())
	{
		ssize_t bytesRead = recv(sock_r, buffer, 65536, 0);
		if (bytesRead < 0)
		{
			printf("Error reading socket.\n");
			break;
		}

		int8_t(*samples)[3] = (int8_t(*)[3])(buffer + 22);
		printBufferAtByte14(buffer);

		tempFrames[i][0][0] = switchByteOrder24((uint32_t)samples[ace_channel][0]);
		tempFrames[i][0][1] = switchByteOrder24((uint32_t)samples[ace_channel][1]);
		tempFrames[i][0][2] = switchByteOrder24((uint32_t)samples[ace_channel][2]);

		i++;
		if (i == 4096)
		{
			drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, 4096, tempFrames);
			if (framesWritten != 4096)
			{
				printf("Failed to write audio frames.\n");
				break;
			}
			i = 0;
		}
	}

	close(sock_r);
	drwav_uninit(&wav);

	return 0;
}