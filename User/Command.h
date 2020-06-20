#ifndef _COMMAND_H
#define _COMMAND_H

#ifndef __stdint_h
#include <stdint.h>
#endif

typedef struct {
	// 输入
	uint8_t *param;		// 指令参数
	int paramLen;		// 指令参数长度
	
	// 输出
	uint8_t *out;	// 响应参数
	int outLen;		// 响应参数长度
} CMDContext;


int ExecCommand(uint8_t cc, CMDContext *ctx);


#endif
