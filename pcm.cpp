#include "pcm.h"


#define PIXEL(frame, x, y) ((*(frame.ptr(x, y)))?1:0)


#define SWAP_PCM_FRAME() memcpy(&(PCMFrame[0]), &(PCMFrame[PCM_HEIGHT]), ((PCM_STAIRS) * PCM_WIDTH_BYTES))

#define L0_POS 0
#define R0_POS 16
#define L1_POS 32
#define R1_POS 48
#define L2_POS 64
#define R2_POS 80
#define P_POS  96
#define Q_POS  112
#define S_POS  112

#define L0 buf[0]
#define R0 buf[1]
#define L1 buf[2]
#define R1 buf[3]
#define L2 buf[4]
#define R2 buf[5]

static uint32_t stairsCount = 0;
static uint32_t crcErrorCount = 0;
static uint32_t parityErrorCount = 0;


static uint8_t PCMFrame[PCM_NTSC_HEIGHT + PCM_STAIRS][PCM_WIDTH_BYTES];

uint16_t buf[6];
uint16_t P;
uint16_t Q;

void readBlock(uint16_t j, bool type){
	L0 = (( PCMFrame[j + L0_POS][0] << 6)          | (PCMFrame[j + L0_POS][1] >> 2)) << 2;
	R0 = (((PCMFrame[j + R0_POS][1] & 0x03) << 12) | (PCMFrame[j + R0_POS][2] << 4) | (PCMFrame[j + R0_POS][3] >> 4)) << 2;
	L1 = (((PCMFrame[j + L1_POS][3] & 0x0f) << 10) | (PCMFrame[j + L1_POS][4] << 2) | (PCMFrame[j + L1_POS][5] >> 6)) << 2;
	R1 = (((PCMFrame[j + R1_POS][5] & 0x3f) << 8)  | (PCMFrame[j + R1_POS][6]     )) << 2;
	L2 = (( PCMFrame[j + L2_POS][7] << 6)          | (PCMFrame[j + L2_POS][8] >> 2)) << 2;
	R2 = (((PCMFrame[j + R2_POS][8] & 0x03) << 12) | (PCMFrame[j + R2_POS][9] << 4) | (PCMFrame[j + R2_POS][10] >> 4)) << 2;
	P  = (((PCMFrame[j + P_POS][10] & 0x0f) << 10) | (PCMFrame[j + P_POS][11] << 2) | ((PCMFrame[j + P_POS][12] >> 6) & 0x03)) << 2;
	Q  = (((PCMFrame[j + Q_POS][12] & 0x3f) << 8) | (PCMFrame[j + Q_POS][13])) << 2;

	// 16 бит PCM
	if (type){
		L0 |= (PCMFrame[j + L0_POS][12] >> 4) & 0x03;
		R0 |= (PCMFrame[j + R0_POS][12] >> 2) & 0x03;
		L1 |= (PCMFrame[j + L1_POS][12] >> 0) & 0x03;
		R1 |= (PCMFrame[j + R1_POS][13] >> 6) & 0x03;
		L2 |= (PCMFrame[j + L2_POS][13] >> 4) & 0x03;
		R2 |= (PCMFrame[j + R2_POS][13] >> 2) & 0x03;
		P  |= (PCMFrame[j +  P_POS][13] >> 0) & 0x03;
	}
}

void writeBlock(uint16_t j, bool type){
	// Не пытайтесь это читать... Сломаетесь...
	PCMFrame[j + L0_POS][ 0]  = L0 >> 8;
	PCMFrame[j + L0_POS][ 1]  = L0 & 0xfc;

	PCMFrame[j + R0_POS][ 1] |= (R0 >> 14) & 0x3;
	PCMFrame[j + R0_POS][ 2]  = (R0 >> 6)  & 0xff;
	PCMFrame[j + R0_POS][ 3]  = (R0 << 2)  & 0xf0;

	PCMFrame[j + L1_POS][ 3] |= (L1 >> 12) & 0x0f;
	PCMFrame[j + L1_POS][ 4]  = (L1 >> 4)  & 0xff;
	PCMFrame[j + L1_POS][ 5]  = (L1 << 6)  & 0xc0;

	PCMFrame[j + R1_POS][ 5] |= (R1 >> 10) & 0x3f;
	PCMFrame[j + R1_POS][ 6]  = (R1 >> 2)  & 0xff;

	PCMFrame[j + L0_POS][ 7]  = L2 >> 8;
	PCMFrame[j + L0_POS][ 8]  = L2 & 0xfc;

	PCMFrame[j + R0_POS][ 8] |= (R2 >> 14) & 0x3;
	PCMFrame[j + R0_POS][ 9]  = (R2 >> 6)  & 0xff;
	PCMFrame[j + R0_POS][10]  = (R2 << 2)  & 0xf0;

	PCMFrame[j + L1_POS][10] |= (P  >> 12) & 0x0f;
	PCMFrame[j + L1_POS][11]  = (P  >> 4)  & 0xff;
	PCMFrame[j + L1_POS][12]  = (P  << 6)  & 0xc0;


	// 16 бит PCM
	if (type){
		PCMFrame[j + L0_POS][12] |= (L0 & 0x03) << 4;
		PCMFrame[j + R0_POS][12] |= (R0 & 0x03) << 2;
		PCMFrame[j + L1_POS][12] |= (L1 & 0x03);
		PCMFrame[j + R1_POS][13] |= (R1 & 0x03) << 6;
		PCMFrame[j + L2_POS][13] |= (L2 & 0x03) << 4;
		PCMFrame[j + R2_POS][13] |= (R2 & 0x03) << 2;
		PCMFrame[j +  P_POS][13] |= (P  & 0x03);
	} else {
		PCMFrame[j + R1_POS][12] |= (Q  >> 10) & 0x3f;
		PCMFrame[j + R1_POS][13]  = (Q  >> 2)  & 0xff;
	}
}

void readPCMFrame(cv::Mat frame, uint8_t offset, bool full){
	int16_t header = 1;
	// перенос
	memcpy(&(PCMFrame[0]), &(PCMFrame[PCM_NTSC_HEIGHT]), ((PCM_STAIRS) * PCM_WIDTH_BYTES));

	if (!full) {
		memset(PCMFrame[PCM_STAIRS], 0, 11 * PCM_WIDTH_BYTES);
		header = 0;
	}

	uint8_t *line;
	int pcmLine = 0;
	for(int j = 0; j < frame.rows; j+=2){
		if (pcmLine >= PCM_NTSC_HEIGHT){
			break;
		}

		uint16_t black = 0;
		uint16_t start = 0;
		uint16_t end = 0;
		float pixelSize = 1;
		bool startSync = false;
		// Поиск начала данных в строке
		for(int i = 0; i < frame.cols; i++){
			if (PIXEL(frame, j + offset, i)){
        start = i;
				if (black)
					startSync = true;
			} else {
				if (startSync){
					start += black + 1;
					break;
				}
				if (start)
					black++;
			}
		}
		if (!start){ // Пустая строка
			continue;
		}

		if (header > 0){
			header--;
			continue;
		}

		// Поиск конца данных в строке
		for(int i = frame.cols - 1; i >= 0; i--){
			if (PIXEL(frame, j + offset, i)){
        end = i;
			} else {
				if (end){
					end -= black;
					break;
				}
			}
		}
		pixelSize = (float)(end - start) / PCM_WIDTH_BITS;
		//printf("%d\t%d\t%.2f\n", start, end, pixelSize);

		// Чтение данных
		if(full)
			line = PCMFrame[pcmLine + PCM_STAIRS];
		else
			line = PCMFrame[pcmLine + PCM_STAIRS + 11];

		memset(line, 0, PCM_WIDTH_BYTES);
		for(int i = 0; i < PCM_WIDTH_BITS; i ++){
			line[i >> 3] |= PIXEL(frame, j + offset, (int)(i * pixelSize + start + pixelSize / 2)) << (7 - (i & 0b111)); // /8 and %8
		}
		uint16_t crcCheck = Calculate_CRC_CCITT(line, 14);
		uint16_t crcOriginal = line[14] << 8 | line[15];
		if (crcCheck != crcOriginal){
			line[14] = 0;
			line[15] = 0;
			//printf("CRC ERROR 0x%04x 0x%04x\n", crcOriginal, crcCheck);
			crcErrorCount++;
		}
		pcmLine++;
	}

	if (!full) {
		// Восстановление потраченных строк
	}
}

#define PCM_PIXEL_0 30
#define PCM_PIXEL_1 127

void writePCMFrame(cv::Mat frame, uint8_t offset, bool full){
	int16_t header = 0;

	//if (!full) {
	//	memset(PCMFrame[PCM_STAIRS], 0, 11 * PCM_WIDTH_BYTES);
	//	header = 0;
	//}

	for(int pcmLine = 0; pcmLine < PCM_NTSC_HEIGHT; pcmLine++){
		uint16_t imageLine = (pcmLine * 2) + offset + header;
		// sync
		frame.at<uchar>(cv::Point(0, imageLine)) = PCM_PIXEL_0;
		frame.at<uchar>(cv::Point(1, imageLine)) = PCM_PIXEL_1;
		frame.at<uchar>(cv::Point(2, imageLine)) = PCM_PIXEL_0;
		frame.at<uchar>(cv::Point(3, imageLine)) = PCM_PIXEL_1;
		frame.at<uchar>(cv::Point(4, imageLine)) = PCM_PIXEL_0;

		// calc CRC
		uint16_t crc = Calculate_CRC_CCITT(PCMFrame[pcmLine], 14);
		PCMFrame[pcmLine][14] = crc >> 8;
		PCMFrame[pcmLine][15] = crc & 0xff;

		// write pixels
		for(int pcmPixel = 0; pcmPixel < PCM_WIDTH_BITS ; pcmPixel++){
			if (PCMFrame[pcmLine + PCM_STAIRS][pcmPixel >> 3] & (1 << (pcmPixel & 0x7)))
				frame.at<uchar>(cv::Point(5 + pcmPixel, imageLine)) = PCM_PIXEL_1;
		}
	}
}

void PCMFrame2wav(SNDFILE *outfile, bool type){
	//printf("\n");
	//printFrame();
	for(int j = 0; j < PCM_NTSC_HEIGHT; j++){
		stairsCount++;
		readBlock(j, type);

		uint8_t crcErrors = 0;
		if (PCMFrame[j + L0_POS][15] == 0){crcErrors++; /*printf("L0\n");*/}
		if (PCMFrame[j + R0_POS][15] == 0){crcErrors++; /*printf("R0\n");*/}
		if (PCMFrame[j + L1_POS][15] == 0){crcErrors++; /*printf("L1\n");*/}
		if (PCMFrame[j + R1_POS][15] == 0){crcErrors++; /*printf("R1\n");*/}
		if (PCMFrame[j + L2_POS][15] == 0){crcErrors++; /*printf("L2\n");*/}
		if (PCMFrame[j + R2_POS][15] == 0){crcErrors++; /*printf("R2\n");*/}
		if (PCMFrame[j +  P_POS][15] == 0){crcErrors++; /*printf(" P\n");*/}
		//if (!PCMFrame[j +  Q_POS][15]) crcErrors++;
		//printf("%d\n", crcErrors);

		uint16_t PC = L0 ^ R0 ^ L1 ^ R1 ^ L2 ^ R2;
		if (P != PC && PCMFrame[j + P_POS][15]/* && crcErrors == 1*/){
			parityErrorCount++;
			if (!PCMFrame[j + L0_POS][15]){L0 = P  ^ R0 ^ L1 ^ R1 ^ L2 ^ R2;}
			if (!PCMFrame[j + R0_POS][15]){R0 = L0 ^ P  ^ L1 ^ R1 ^ L2 ^ R2;}
			if (!PCMFrame[j + L1_POS][15]){L1 = L0 ^ R0 ^ P  ^ R1 ^ L2 ^ R2;}
			if (!PCMFrame[j + R1_POS][15]){R1 = L0 ^ R0 ^ L1 ^ P  ^ L2 ^ R2;}
			if (!PCMFrame[j + L2_POS][15]){L2 = L0 ^ R0 ^ L1 ^ R1 ^ P  ^ R2;}
			if (!PCMFrame[j + R2_POS][15]){R2 = L0 ^ R0 ^ L1 ^ R1 ^ L2 ^ P ;}

		}/* else if (crcErrors > 1){
			memset(buf, 0, sizeof(uint16_t) * 6);
			//printf("%d\n", crcErrors);

			}*/

		sf_write_short(outfile, (int16_t *)buf, 6);
	}
}

bool wav2PCMFrame(SNDFILE *infile, bool type){
	// Перенос конца прошлого блока в начало текущего
	memcpy(&(PCMFrame[0]), &(PCMFrame[PCM_NTSC_HEIGHT]), ((PCM_STAIRS) * PCM_WIDTH_BYTES));

	// Заполнение
	for(int j = 0; j < PCM_NTSC_HEIGHT; j++){
		sf_count_t count = sf_read_short(infile, (int16_t *)buf, 6);
		if (count != 6){
			printf("%d\n", count);
			return(false);
		}
		P = L0 ^ R0 ^ L1 ^ R1 ^ L2 ^ R2;
		writeBlock(j, type);
	}
	return true;
}

uint32_t samplesCount(void){
	return(stairsCount * 3);
}

void printFrame(void){
	for(int j = 0; j < PCM_NTSC_HEIGHT + PCM_STAIRS; j++){
		for(int i = 0; i < PCM_WIDTH_BYTES; i++){
			fprintf(stderr, "0x%02x ", PCMFrame[j][i]);
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");
}

void showStatistics(void){
	fprintf(stderr, "Stairs count: %d\n", stairsCount);
	fprintf(stderr, "CRC errors count: %d\n", crcErrorCount);
	fprintf(stderr, "Parity errors count: %d\n", parityErrorCount);
}
