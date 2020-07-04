; haribote-os
; TAB = 4

BOTPAK	 EQU  0x00280000
DSKCAC	 EQU  0x00100000		
DSKCAC0	 EQU  0x00008000
; BOOT_INFO關係
CYLS     EQU  0x0ff0 ; 設定開機磁區
LEDS     EQU  0x0ff1
VMODE    EQU  0x0ff2 ; 與色彩有關的資訊。 表示顏色所會用到的位元數
VBEMODE  EQU  0x105  ; 1024 * 768  8bit 彩色模式
SCRNX    EQU  0x0ff4 ; X方向的解析度
SCRNY    EQU  0x0ff6 ; Y方向的解析度
VRAM     EQU  0x0ff8 ; 圖形緩衝區( Graphic Buffer ) 的開始位址 
    
    ORG    0xc200        ;  程式碼讀到0xc200
;   設定畫面模式
    MOV    AL, 0x13      ; 320x200 8bit彩色
    MOV    AH, 0x00
    INT    0x10
    MOV	   BYTE  [VMODE], 8	; 紀錄畫面模式
	MOV	   WORD  [SCRNX], 320
	MOV	   WORD  [SCRNY], 200
	MOV	   DWORD [VRAM], 0x000a0000

keystatus:   
    ; 從BIOS告知鍵盤的LED狀態
	MOV		AH,0x02
	INT		0x16 			; keyboard BIOS
	MOV		[LEDS],AL

	; 使用IMR禁止PIC0、PIC1中斷
    MOV		AL,0xff
	OUT		0x21,AL
	NOP
	OUT		0xa1,AL

	CLI

	; 啟動A20匯流排，從16位元的1MB到32位元
	CALL	waitkbdout
	MOV		AL,0xd1
	OUT		0x64,AL
	CALL	waitkbdout
	MOV		AL,0xdf			; enable A20
	OUT		0x60,AL
	CALL	waitkbdout



[INSTRSET "i486p"]				

	LGDT	[GDTR0]	         ; 設定暫定GDT	
	MOV		EAX,CR0
	AND		EAX,0x7fffffff	 ; 將bit31設為0(為了禁止分頁)
	OR		EAX,0x00000001	 ; 將bit0設為1(為了轉移至保護模式)
	MOV		CR0,EAX          ; CR0: 控制卡暫存器0
	JMP		pipelineflush    ; CPU使用管線解譯指令，必須使用JMP來重新開始
pipelineflush:
	MOV		AX,1*8           ; gdt + 1所代表區段的意思
	MOV		DS,AX
	MOV		ES,AX
	MOV		FS,AX
	MOV		GS,AX
	MOV		SS,AX

	; bootpack的轉送
	MOV		ESI,bootpack ; 轉送來源 asmhead.nas最底部
	MOV		EDI,BOTPAK   ; 轉送目的地
	MOV		ECX,512*1024/4 ; 一次搬移Double Word所以要除以4
	CALL	memcpy

	; 將開機磁區轉移到1MB以後的位置
	MOV		ESI,0x7c00
	MOV		EDI,DSKCAC
	MOV		ECX,512/4
	CALL	memcpy

	; 開機磁區以後的內容(0x00008200)複製到 0x00100200
	MOV		ESI,DSKCAC0+512
	MOV		EDI,DSKCAC+512
	MOV		ECX,0
	MOV		CL,BYTE [CYLS]
	IMUL	ECX,512*18*2/4
	SUB		ECX,512/4
	CALL	memcpy

	; asmhead任務結束，開始使用bootpack
	; bootpack啟動
	MOV		EBX,BOTPAK
	MOV		ECX,[EBX+16] ; [EBX+16] : 0x11a8
	ADD		ECX,3			
	SHR		ECX,2
	JZ		skip
	MOV		ESI,[EBX+20] ; [EBX+20] : 0x10c8
	ADD		ESI,EBX
	MOV		EDI,[EBX+12] ; [EBX + 12] : 0x00310000
	CALL	memcpy
skip:
	MOV		ESP,[EBX+12]
	JMP		DWORD 2*8:0x0000001b ; 從區段2開始執行

waitkbdout:
	IN		 AL,0x64
	AND		 AL,0x02
	JNZ		waitkbdout
	RET

memcpy:
	MOV		EAX,[ESI]
	ADD		ESI,4
	MOV		[EDI],EAX
	ADD		EDI,4
	SUB		ECX,1
	JNZ		memcpy
	RET

	ALIGNB	16
GDT0:         ; 無效的區段，用於初始而已
	RESB	8 ; 空的區段
	DW		0xffff,0x0000,0x9200,0x00cf ; 可讀取的32個bit
	DW		0xffff,0x0000,0x9a28,0x0047 ; 可執行的32個bit(bootpack用)
	DW		0
GDTR0:
	DW		8*3-1
	DD		GDT0
	ALIGNB	16
bootpack:
