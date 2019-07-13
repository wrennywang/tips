// IndexCheck.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>

typedef uint64_t            SmpUlongT;

typedef enum FrameTypeS {
	SMP_P_FREAM = 0,
	SMP_I_FREAM,

	SMP_G711A_AUDIO,
	SMP_G711MU_AUDIO,
}FrameTypeT;

typedef struct FrameIndexS
{
	int FrameType;                  // 帧类型
	int FrameIndex;                 // 帧索引
	int FramePos;                   // 帧偏移
	int FrameSize;                  // 帧大小
	SmpUlongT TimeStamp;            // 时间戳

}FrameIndexT;

typedef struct SmpVodfileIndexS
{

	int IndexSize;

	int FrameCount;                 // 总帧数？

	int CodecType;                  // 编码类型


	FrameIndexT FrameIndex[];


}SmpVodfileIndexT;

// 查找数据中naltype 7/5/1；当码流中出现，则说明为一帧起始，需注意有7时，必定有8，5，需将7当I帧起始，否则将5当I帧起始；1为P帧起始；
static bool IsNalHead(char *buf, int *type)
{
	bool isHead = false;
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
	{
		int nalType = buf[4] & 0x1F;
		if (nalType == 7 || nalType == 5 || nalType == 1)
		{
			*type = nalType;
			isHead = true;
		}
	}
	return isHead;
}

// 校验索引文件和数据文件，按正常逻辑，当前帧的偏移为上一帧偏移加长度，且偏移开始为4字节为00 00 00 01，第5字节为naltype;
static bool IsNeedReIndex(FILE *tStream, SmpVodfileIndexT *vodfileIndex)
{
	bool bNeedReIndex = false;
	fseek(tStream, 0, SEEK_END);
	const int filelen = ftell(tStream);

	const int BUF_LEN = 1024 * 1024;
	char *buf = (char*)malloc(BUF_LEN);
	int i = 0;
	int readPos = 0;
	int pos = vodfileIndex->FrameIndex[0].FramePos;
	int size = vodfileIndex->FrameIndex[0].FrameSize;
	int frameNum = vodfileIndex->FrameIndex[0].FrameIndex;
	SmpUlongT nInterval = 0;
	const int totalFrames = vodfileIndex->FrameCount;

	if ((size <= 5) || (pos + size > filelen))
	{
		free(buf);
		return bNeedReIndex = true;
	}
	if (pos != 0)
	{
		printf("vod file: first frame pos %d\n", pos);
	}

	fseek(tStream, pos, SEEK_SET);
	int readlen = fread(buf, 1, BUF_LEN, tStream);
	bool bFileEnd = (readlen != BUF_LEN);
	int nalType = 0;
	if (!IsNalHead(buf, &nalType))
	{
		free(buf);
		return true;
	}

	while (readlen)
	{

		i++;
		if (i == totalFrames)
		{
			if (readlen != size)
			{
				printf("vod file: file has some error, last frame size %d, file remain size %d\n", size, readlen);
			}
			free(buf);
			return false;
		}
		if (i > totalFrames)
		{
			free(buf);
			return true;
		}

		if (size + 5 < readlen)
		{
			// 偏移加长度超出下一帧的偏移
			if ((pos + size) > vodfileIndex->FrameIndex[i].FramePos)
			{
				if (i < totalFrames && vodfileIndex->FrameIndex[i++].FrameSize == 0)
				{
					free(buf);
					return false;
				}
				printf( "vod file: cur frame pos (%d) + size (%d) is larger than next frame pos(%d)!\n",
					pos, size, vodfileIndex->FrameIndex[i].FramePos);
				free(buf);
				return true;
			}
			if (frameNum + 1 != vodfileIndex->FrameIndex[i].FrameIndex)
			{
				printf( "vod file: cur frame index (%d) and next frame index isn't continue\n",
					frameNum, vodfileIndex->FrameIndex[i].FrameIndex);
				free(buf);
				return true;
			}

			if (IsNalHead(buf + size, &nalType))
			{
				// 检验完一帧挪动一帧，代码逻辑简单，但性能低
				int remainLen = readlen - size;
				memmove(buf, buf + size, remainLen);
				readlen = fread(buf + remainLen, 1, BUF_LEN - remainLen, tStream);
				if ((readlen > 0) && (readlen <= (BUF_LEN - remainLen)))
				{
					readlen += remainLen;
				}
				if (readlen == 0)
				{
					printf("vod file: file end! index file record %d frame, data file %d frame\n", totalFrames, frameNum);
					free(buf);
					return false;
				}
			}
			else
			{
				free(buf);
				return true;
			}

			pos = vodfileIndex->FrameIndex[i].FramePos;
			size = vodfileIndex->FrameIndex[i].FrameSize;
			frameNum = vodfileIndex->FrameIndex[i].FrameIndex;
			if (i > 0)
			{
				nInterval = vodfileIndex->FrameIndex[i].TimeStamp - vodfileIndex->FrameIndex[i - 1].TimeStamp;
				printf("frame %d interval %lldd\n", i, nInterval);
			}
			if (i > 0 && vodfileIndex->FrameIndex[i - 1].FramePos + vodfileIndex->FrameIndex[i - 1].FrameSize != vodfileIndex->FrameIndex[i].FramePos)
			{
				printf("frame %d, pos %d, size %d\n", frameNum, pos, size);
			}
		}

	}

	free(buf);
	return true;
}

// 根据文件重建索引，返回索引缓冲，需要在相关处理完后释放！！！
static SmpVodfileIndexT* ReIndex(FILE *pf)
{
	SmpVodfileIndexT* vodfileIndex = (SmpVodfileIndexT*)malloc(sizeof(SmpVodfileIndexT) + 40 * 60 * 30 * sizeof(FrameIndexT));
	
	fseek(pf, 0, SEEK_SET);
	const int BUF_LEN = 1024 * 1024;
	char *buf = (char*)malloc(BUF_LEN + 5);
	int readLen = 0;
	int fileReadPos = 0;
	int frameNum = 0;
	int timeStamp = 0;
	int nalType = 0;
	bool hasSPS = false;
	bool bRemain5Byte = false;
	do
	{
		readLen = fread(buf + (bRemain5Byte ? 5 : 0), 1, BUF_LEN, pf);
		int pos = 0;
		while (pos + 5 < readLen + (bRemain5Byte ? 5 : 0))
		{
			if (IsNalHead(buf + pos, &nalType))
			{
				if (nalType == 7)
				{
					hasSPS = true;
				}
				if (hasSPS && nalType != 7 && nalType != 1)
				{
					pos++;
					continue;
				}
				if (!hasSPS && nalType != 5 && nalType != 1)
				{
					pos++;
					continue;
				}

				vodfileIndex->FrameIndex[frameNum].FrameIndex = frameNum;
				vodfileIndex->FrameIndex[frameNum].FramePos = fileReadPos + pos - (bRemain5Byte ? 5 : 0);
				if (frameNum > 0)
				{
					vodfileIndex->FrameIndex[frameNum - 1].FrameSize = fileReadPos + pos - (bRemain5Byte ? 5 : 0) - vodfileIndex->FrameIndex[frameNum - 1].FramePos;
				}
				else
				{
					vodfileIndex->FrameIndex[frameNum].FrameSize = 0;
				}
				vodfileIndex->FrameIndex[frameNum].FrameType = (nalType == 5 || nalType == 7) ? SMP_I_FREAM : SMP_P_FREAM;
				// 先按25帧写时间戳，间隔需要根据实际单位进行调整
				vodfileIndex->FrameIndex[frameNum].TimeStamp = timeStamp + 3600;
				timeStamp = vodfileIndex->FrameIndex[frameNum].TimeStamp;
				if (frameNum > 0 
					&& (vodfileIndex->FrameIndex[frameNum].FramePos 
						!= vodfileIndex->FrameIndex[frameNum - 1].FramePos + vodfileIndex->FrameIndex[frameNum - 1].FrameSize))
				printf("cur frame pos %d, last frame pos %d size %d >> %d \n", 
					vodfileIndex->FrameIndex[frameNum].FramePos,
					vodfileIndex->FrameIndex[frameNum - 1].FramePos, vodfileIndex->FrameIndex[frameNum-1].FrameSize,
					vodfileIndex->FrameIndex[frameNum - 1].FramePos + vodfileIndex->FrameIndex[frameNum - 1].FrameSize);
				frameNum++;
				if (frameNum > 40 * 60 * 30)
				{
					goto END;
				}
			}
			pos++;
		}
		memmove(buf, buf + BUF_LEN - (bRemain5Byte ? 5 : 0), 5);
		bRemain5Byte = true;
		
		fileReadPos += readLen;
	} while (readLen > 0 && readLen <= BUF_LEN);
END:

	vodfileIndex->FrameIndex[frameNum].FrameSize = fileReadPos - vodfileIndex->FrameIndex[frameNum - 1].FramePos;
	vodfileIndex->FrameCount = frameNum;
	vodfileIndex->IndexSize = sizeof(SmpVodfileIndexT) + frameNum * sizeof(FrameIndexT);
	vodfileIndex->CodecType = 1;
	printf("vod file: reindex; total frame %d, \n", frameNum);
	int fileLen = ftell(pf);

	free(buf);
	return vodfileIndex;
}


int main()
{
	FILE *fpIndex;

	errno_t err = fopen_s(&fpIndex, "C:\\Users\\admin\\Desktop\\ehualu\\jy_video\\2019-07-02_11-48-26.ts.index", "rb");

	if (err == 0)
	{
		FILE *fpData;
		err = fopen_s(&fpData, "C:\\Users\\admin\\Desktop\\ehualu\\jy_video\\2019-07-02_11-48-26.ts", "rb");
		if (err)
		{
			printf("data file open fail!\n");
		}
		char *buffer = (char*)malloc(1024 * 1024);
		int len = fread(buffer, 1, 1024 * 1024, fpIndex);
		len = ftell(fpIndex);
		int size = sizeof(SmpVodfileIndexT);
		SmpVodfileIndexT *pVodFileIndex = (SmpVodfileIndexT *)buffer;
		if (IsNeedReIndex(fpData, pVodFileIndex))
		{
			pVodFileIndex = ReIndex(fpData);
			if (IsNeedReIndex(fpData, pVodFileIndex))
			{
				printf("something error too!\n");
			}
			else
			{
				printf("reindex sucess!\n");
			}
			free(pVodFileIndex);
		}
		else
		{
			printf("file ok! no need reindex!!\n");
		}
		free(buffer);
	}
	else
	{
		printf("index file open fail!\n");
	}
	while (getchar() != 'q')
	{
	}
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
