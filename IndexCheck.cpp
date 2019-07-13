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

// 查找数据中naltype 7/5/1；
// 当码流中出现，则说明为一帧起始，需注意有7时，必定有8，5，需将7当I帧起始，否则将5当I帧起始；1为P帧起始；
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

// 校验索引文件和数据文件，
// 按正常逻辑，当前帧的偏移为上一帧偏移加长度，且偏移开始为4字节为00 00 00 01，第5字节为naltype;
static bool IsNeedReIndex(FILE *tStream, SmpVodfileIndexT *vodfileIndex)
{
	bool bNeedReIndex = false;
	fseek(tStream, 0, SEEK_END);
	const int filelen = ftell(tStream);

	const int BUF_LEN = 1024 * 1024;
	char *buf = (char*)malloc(BUF_LEN);

	// 当前文件中帧索引值
	int nCurFileFrameIndex = 0;
	int readPos = 0;
	int pos = vodfileIndex->FrameIndex[0].FramePos;
	int size = vodfileIndex->FrameIndex[0].FrameSize;
	
	// 上一帧帧序号
	int nLastFrameNO = vodfileIndex->FrameIndex[0].FrameIndex;
	SmpUlongT nInterval = 0;
	
	// 索引信息头中总帧数
	const int cnTotalFrames = vodfileIndex->FrameCount;

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

		nCurFileFrameIndex++;
		if (nCurFileFrameIndex == cnTotalFrames
			|| vodfileIndex->IndexSize < nCurFileFrameIndex * sizeof(FrameIndexT) + sizeof(SmpVodfileIndexT))
		{
			if (readlen != size)
			{
				printf("vod file: file has some error, last frame size %d, file remain size %d\n", size, readlen);
			}
			free(buf);
			return false;
		}

		if (size + 5 < readlen)
		{
			// 上一帧偏移(pos)加长度(size)不等于当前帧的偏移
			if ((pos + size) != vodfileIndex->FrameIndex[nCurFileFrameIndex].FramePos)
			{
				// 当前帧索引值小于索引信息头中总帧数，但下一帧长度已经为0，有异常，但认为到当前文件结束；
				if (nCurFileFrameIndex < cnTotalFrames && vodfileIndex->FrameIndex[nCurFileFrameIndex++].FrameSize == 0)
				{
					free(buf);
					return false;
				}
				printf( "vod file: cur frame pos (%d) + size (%d) is larger than next frame pos(%d)!\n",
					pos, size, vodfileIndex->FrameIndex[nCurFileFrameIndex].FramePos);
				free(buf);
				return true;
			}
			// 帧不连续
			if (nLastFrameNO + 1 != vodfileIndex->FrameIndex[nCurFileFrameIndex].FrameIndex)
			{
				printf( "vod file: cur frame index (%d) and next frame index isn't continue\n",
					nLastFrameNO, vodfileIndex->FrameIndex[nCurFileFrameIndex].FrameIndex);
				free(buf);
				return true;
			}

			// 检验数据文件，当前帧位置加长度，是否为下一帧开始(0x00 00 00 01)
			if (IsNalHead(buf + size, &nalType))
			{
				// 检验完一帧挪动一帧，代码逻辑简单，但性能低
				// TODO:优化性能
				int remainLen = readlen - size;
				memmove(buf, buf + size, remainLen);
				readlen = fread(buf + remainLen, 1, BUF_LEN - remainLen, tStream);
				if ((readlen > 0) && (readlen <= (BUF_LEN - remainLen)))
				{
					readlen += remainLen;
				}
				if (readlen == 0)
				{
					printf("vod file: file end! index file record %d frame, data file %d frame\n", cnTotalFrames, nLastFrameNO);
					free(buf);
					return false;
				}
			}
			else
			{
				bool bIndexError = false;
				int i = 0;
				for (i = nCurFileFrameIndex; i < cnTotalFrames - 1; i++)
				{
					if (vodfileIndex->FrameIndex[i].FrameSize + vodfileIndex->FrameIndex[i].FramePos
						!= vodfileIndex->FrameIndex[i + 1].FramePos)
					{
						printf("vod file index error: frame no %d, frame pos %d, frame size %d, next frame pos %d != (cur frame pos+size = %d) \n",
							i, vodfileIndex->FrameIndex[i].FramePos,
							vodfileIndex->FrameIndex[i].FrameSize, vodfileIndex->FrameIndex[i + 1].FramePos,
							vodfileIndex->FrameIndex[i].FrameSize + vodfileIndex->FrameIndex[i].FramePos);
						bIndexError = true;
						break;
					}
				}
				if (bIndexError)
				{
					printf("vod file: index error at %d; index and data not match at %d\n ", i, nCurFileFrameIndex);
				}
				else
				{
					printf("vod file: index and data not match at %d, but index is ok\n", nCurFileFrameIndex);
				}
				free(buf);
				return true;
			}

			pos = vodfileIndex->FrameIndex[nCurFileFrameIndex].FramePos;
			size = vodfileIndex->FrameIndex[nCurFileFrameIndex].FrameSize;
			nLastFrameNO = vodfileIndex->FrameIndex[nCurFileFrameIndex].FrameIndex;

			// 简单查看帧间隔
			if (0 && nCurFileFrameIndex > 0)
			{
				nInterval = vodfileIndex->FrameIndex[nCurFileFrameIndex].TimeStamp - vodfileIndex->FrameIndex[nCurFileFrameIndex - 1].TimeStamp;
				printf("frame %d interval %lld\n", nCurFileFrameIndex, nInterval);
			}
			// 校验索引，上一帧索引偏移+长度是否等于当前帧索引位置
			if (nCurFileFrameIndex > 0 
				&& vodfileIndex->FrameIndex[nCurFileFrameIndex - 1].FramePos 
				+ vodfileIndex->FrameIndex[nCurFileFrameIndex - 1].FrameSize 
				!= vodfileIndex->FrameIndex[nCurFileFrameIndex].FramePos)
			{
				printf("frame %d, pos %d, size %d\n", nLastFrameNO, pos, size);
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
	int fileReadPos = 0;						// 文件读到的位置
	int nFrameNO = 0;							// 当前帧序号
	int timeStamp = 0;							// 当前帧时间戳
	int nalType = 0;
	bool hasSPS = false;
	int nRemainByte = 0;						// 一次读1M数据，留5个字节与下一次读取数据一起解析
	do
	{
		readLen = fread(buf + nRemainByte, 1, BUF_LEN, pf);
		int pos = 0;
		while (pos + 5 < readLen + nRemainByte)
		{
			if (IsNalHead(buf + pos, &nalType))
			{
				if (nalType == 7)
				{
					hasSPS = true;
				}
				// P slice为P帧开始；读到SPS，则认为sps为I帧开始；没有读到SPS则认为I slice为I帧开始;
				// TODO:当前认为视频帧均为单slice构成
				if ((hasSPS && nalType != 7 && nalType != 1) || (!hasSPS && nalType != 5 && nalType != 1))
				{
					pos++;
					continue;
				}

				vodfileIndex->FrameIndex[nFrameNO].FrameIndex = nFrameNO;
				vodfileIndex->FrameIndex[nFrameNO].FramePos = fileReadPos + pos - nRemainByte;

				// 读一帧，计算上一帧大小
				if (nFrameNO > 0)
				{
					vodfileIndex->FrameIndex[nFrameNO - 1].FrameSize = fileReadPos + pos - nRemainByte- vodfileIndex->FrameIndex[nFrameNO - 1].FramePos;
				}
				else
				{
					vodfileIndex->FrameIndex[nFrameNO].FrameSize = 0;
				}
				vodfileIndex->FrameIndex[nFrameNO].FrameType = (nalType == 5 || nalType == 7) ? SMP_I_FREAM : SMP_P_FREAM;

				// 先按25帧写时间戳，间隔需要根据实际单位进行调整
				vodfileIndex->FrameIndex[nFrameNO].TimeStamp = timeStamp;
				timeStamp = vodfileIndex->FrameIndex[nFrameNO].TimeStamp + 3600;
				if (nFrameNO > 0
					&& (vodfileIndex->FrameIndex[nFrameNO].FramePos
						!= vodfileIndex->FrameIndex[nFrameNO - 1].FramePos + vodfileIndex->FrameIndex[nFrameNO - 1].FrameSize))
				{
					printf("cur frame pos %d, last frame pos %d size %d >> %d \n",
						vodfileIndex->FrameIndex[nFrameNO].FramePos,
						vodfileIndex->FrameIndex[nFrameNO - 1].FramePos, vodfileIndex->FrameIndex[nFrameNO - 1].FrameSize,
						vodfileIndex->FrameIndex[nFrameNO - 1].FramePos + vodfileIndex->FrameIndex[nFrameNO - 1].FrameSize);
				}
				nFrameNO++;

				// TODO:当前按半个小时录一个文件处理，先假定文件中帧数不会大于40*60*30
				if (nFrameNO > 40 * 60 * 30)
				{
					goto END;
				}
			}
			pos++;
		}
		memmove(buf, buf + BUF_LEN - nRemainByte, 5);
		nRemainByte = 5;
		
		fileReadPos += readLen;
	} while (readLen > 0 && readLen <= BUF_LEN);
END:

	vodfileIndex->FrameIndex[nFrameNO].FrameSize = fileReadPos - vodfileIndex->FrameIndex[nFrameNO - 1].FramePos;
	vodfileIndex->FrameCount = nFrameNO;
	vodfileIndex->IndexSize = sizeof(SmpVodfileIndexT) + nFrameNO * sizeof(FrameIndexT);
	vodfileIndex->CodecType = 1;
	printf("vod file: reindex; total frame %d, \n", nFrameNO);
	int fileLen = ftell(pf);

	free(buf);
	return vodfileIndex;
}


int main(int argc, char *argv[])
{
	char indexfileName[_MAX_FNAME + _MAX_PATH] = { 0 };
	char* inputfileName = NULL;
	if (argc == 2)
	{
		inputfileName = argv[1];
		sprintf_s(indexfileName, _MAX_FNAME + _MAX_PATH, "%s.index", inputfileName);
	}
	else
	{
		printf("please input one vod file *.ts\n");
		return 0;
	}
	FILE *fpIndex;

	// 测试情况:
	// jy_video\\2019-07-03_13-44-22.ts 索引文件本身正常，数据文件正常，但两者不匹配
	//			2019-07-02_11-48-26.ts 索引/数据文件均正常
	// 2019-07-10_13-34-21.ts，数据文件正常，索引正常， 但索引26118帧，数据文件仅26071帧
	// 现场姜堰\\2019-06-27_17-27-51.ts,索引文件数据文件不匹配
	// 现场遵义\\2019-07-03_14-25-31.ts,索引文件数据文件正常匹配
	errno_t err = fopen_s(&fpIndex, indexfileName, "rb");

	if (err == 0)
	{
		FILE *fpData;
		err = fopen_s(&fpData, inputfileName, "rb");
		if (err)
		{
			printf("data file open fail!\n");
			return 0;
		}
		fseek(fpIndex, 0, SEEK_END);
		int len = ftell(fpIndex);
		fseek(fpIndex, 0, SEEK_SET);
		char *buffer = (char*)malloc(len);
		len = fread(buffer, 1, len, fpIndex);
		//int size = sizeof(SmpVodfileIndexT);
		SmpVodfileIndexT *pVodFileIndex = (SmpVodfileIndexT *)buffer;
		if (pVodFileIndex->IndexSize != len)
		{
			printf("vod file: index file size error! head record size %d, in fact size %d, total frames %d\n", 
				pVodFileIndex->IndexSize, len, pVodFileIndex->FrameCount);
			pVodFileIndex->IndexSize = len;
		}
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
	printf("enter 'q' to exit\n");
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
