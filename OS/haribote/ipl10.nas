; haribote-ipl
; TAB = 4

CYLS    EQU    10       ; 讀取磁柱的數量

	ORG    0x7c00       ; 開機磁區512位元組，從0x7c00是規定主要避開BIOS中斷向量
	JMP	   entry
	DB     0x90
	DB 	   "HARIBOTE"	; 可隨意取的開機磁區名稱( 8位元組 )
	DW	   512			; 1磁區的大小 ( 必須是512 )
	DB	   1			; 叢集的大小 ( 必須是1磁區 )
	DW	   1			; FAT要從哪裡開始( 通常由第1磁區開始 )
	DB	   2			; FAT的個數( 必須是2 )
	DW	   224			; 根目錄範圍的大小( 普通是設成224 )
	DW	   2880			; 這個磁碟機的大小( 必須是2880磁區 )
	DB	   0Xf0			; 媒體的格式 ( 必須是0xf0 )
	DW	   9			; FAT區域範圍的長度( 必須是9磁區 )
	DW	   18			; 1磁軌有幾個磁區 ( 必須是18 )
	DW	   2			; 磁頭個數( 必須是2 )
	DD	   0			; 不使用磁碟分割，此處必須是0
	DD	   2880		    ; 這個磁碟機大小再寫一次
	DB	   0,0,0x29	    ; 應該是寫入這個值比較好
	DD	   0xffffffff	; 磁碟區序號
	DB	   "HARIBOTEOS " ;磁碟片名稱( 11個位元組 )
	DB	   "FAT12   "   ; 格式名稱( 8個位元組 )
	RESB   18           ; 暫時騰出18個位元組
	 
; 程式本身
entry:
	MOV    AX, 0        ; 暫存器初始化
	MOV    SS, AX
	MOV    SP, 0x7c00
	MOV    DS, AX

	MOV    AX, 0x0820
	MOV    ES, AX
	MOV    CH, 0        ; 磁柱0
	MOV    DH, 0        ; 磁頭0
	MOV    CL, 2        ; 磁區2

readloop:
	MOV    SI, 0        ; 計算失敗次數的暫存器
retry:
	MOV    AH, 0x02     ; AH = 0x02 : 讀入磁碟片
	MOV    AL, 1        ; 1磁區
	MOV    BX, 0        ; 讀入記憶體的位址(ES * 16 + BX) [ES:BX] 0x8200
	MOV    DL, 0x00     ; A磁碟機
	INT    0x13         ; 呼叫磁碟片BIOS
	JNC    next         ; 沒有錯誤繼續讀取下一磁區
	ADD    SI, 1        ; SI += 1
	CMP    SI, 5        ; SI 與 5 比較
	JAE    error        ; 讀取錯誤超過五次跳到error
	MOV    AH, 0x00     ; AH = 0x00 : 復位磁碟驅動器
	MOV    DL, 0x00     ; A磁碟機
	INT    0x13         ; 磁碟機重設
	JMP    retry
next:
	MOV    AX, ES       ; 將位址推進0x20個位元組
	ADD    AX, 0x0020
	MOV    ES, AX       ; 因為沒有 ADD ES, 0x0020 指令
	ADD    CL, 1        ; CL += 1
	CMP    CL, 18       ; CL 與 18 比較
	JBE    readloop     ; 小於18，跳到readloop
	MOV	   CL, 1        ; 從磁區1開始
	ADD	   DH, 1        ; 切換磁頭1
	CMP	   DH, 2
	JB	   readloop 
	MOV	   DH, 0        ; 切換磁頭0
	ADD	   CH, 1        ; 磁柱 += 1
	CMP	   CH, CYLS     ; 小於10個磁柱
	JB	   readloop
	
; 跳到haribote.sys執行
	MOV		[0x0ff0], CH ; 告訴IPL程式磁碟片已讀到哪個磁柱
	JMP		0xc200

error:
	MOV    SI, msg

putloop:
	MOV    AL, [SI]
	ADD    SI, 1        ; SI += 1
	CMP    AL, 0
	JE     fin
	MOV    AH, 0x0e     ; 一個文字代表功能
	MOV    BX, 15       ; 顏色代碼 ( color code )
	INT    0x10         ; 呼叫視訊BIOS(中斷interapt)
	JMP    putloop

fin:
	HLT
	JMP fin

; 訊息本身
msg:
	DB	   0x0a, 0x0a ; 2個換行
	DB	   "load error"
	DB	   0x0a ; 換行
	DB	   0

	RESB   0x7dfe-$ ; 填入0x00直到0x7dfe( 0x7c00 + 0x1fe )為止的命令
	
	DB     0x55, 0xaa ; 必須位於開機磁區(512個位元組)的第510個位元組上
	

