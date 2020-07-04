[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "crack7.nas"]

        GLOBAL _HariMain

[SECTION .text]

_HariMain:
        MOV     AX, 1005 * 8
        MOV     DS, AX
        CMP     DWORD [DS:0x0004], 'Hari'
        JNE     fin                      ; 因為不是應用程式，所以甚麼都不用做

        MOV     ECX, [DS:0x0000]         ; 讀取這個應用程式的資料區段大小
        MOV     AX, 2005 * 8
        MOV     DS, AX

crackloop:
        ADD     ECX, -1                  ; 用123填滿
        MOV     BYTE [DS:ECX], 123
        CMP     ECX, 0
        JNE     crackloop
fin:
        MOV     EDX, 4
        INT     0x40