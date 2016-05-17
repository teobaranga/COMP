.286
.model huge
.stack 100h
.data
    PENCOLOR    db 1100b
    FILLCOLOR   db 3
    winA        dw 0A000h
    xRes        dw 0
    yRes        dw 0
    bpp         dw 2        ; bits per pixel
    gran        dw 0        ; granularity
    pixelColor  db 0        ; the pixel color returned from getPixelColor
    mode        dw 0        ; mode number
    WinFuncPtr  dw 0        ; pointer to the code that changes the window (double)
    
    msg_mode db "Enter a mode number (in hex): ",0
    msg_color1 db "Enter the 1st color: ",0
    msg_color2 db "Enter the 2nd color: ",0

.data?
    buffer db 256 DUP(?)

.code
start:
    mov ax, @data
    mov ds, ax
    
    ; get mode
    mov ax, OFFSET msg_mode
    push ax
    call puts
    add sp, 2
    
    call getinthex
    mov mode, ax
    
    ; get pen color
    mov ax, OFFSET msg_color1
    push ax
    call puts
    add sp, 2
    
    call getint
    xor ah, ah
    mov PENCOLOR, al
    
    ; get fill color
    mov ax, OFFSET msg_color2
    push ax
    call puts
    add sp, 2
    
    call getint
    xor ah, ah
    mov FILLCOLOR, al
    
    ; set mode
    push mode
    call setmode
    add sp, 2
    
    ; set pen color
    xor bx, bx
    mov bl, PENCOLOR
    push bx
    call setpencolor
    add sp, 2
    
    ; draw a triangle with hardcoded coordinates
    ; first side of the triangle
    push 200
    push 100
    push 150
    push 150
    call drawline
    add sp, 8
    
    ; second side
    push 200
    push 100
    push 250
    push 150
    call drawline
    add sp, 8
    
    ; third side
    push 150
    push 150
    push 250
    push 150
    call drawline
    add sp, 8

    ; fill triangle
    push 200
    push 125
    call simplefill
    add sp, 4
    
.end:
    mov ah, 4ch
    int 21h

; Get a character from the user while displaying it as it was typed
; al = getche()
getche:
    mov ah, 1       ; STDIN with echo
    int 21h         ; result in al
    ret

; Print a character to the screen
; void putch(c : byte) 
putch:
    push bp
    mov bp, sp
    
    mov dx, [bp+4]  ; get the character to be printed
    mov ah, 6       ; 6 = console output
    int 21h
    
    pop bp
    ret

; Print a string to the screen
; void puts(s : string)
puts:
    push bp
    mov bp, sp
    
    push bx
    
    mov bx, [bp+4]      ; get the string to be printed
    
.puts_loop:
    mov al, [bx]        ; get the next character
    cmp al, 0           ; terminate if reached the null character
    je .puts_done

    mov ah, 0           ; make sure it's a byte
    push ax             ; print the character
    call putch
    add sp, 2

    inc bx              ; next char
    jmp .puts_loop
    
.puts_done:
    pop bx
    pop bp
    ret

; Get an int in decimal format
; al = getint()
getint:
    push bp
    mov bp, sp
    
    push cx
    
    xor cx, cx      ; hold sum
    mov bl, 10      ; base 10

.gi_loop:           ; loop to get all digits
    call getche

    cmp al, 13      ; end input when user pressed enter
    je .gi_done

    xor ah, ah      ; convert to int
    sub al, '0'
    mov dx, ax      ; multiply current sum by 10 and add the read number
    mov ax, cx
    mul bl
    add ax, dx
    mov cx, ax
    jmp .gi_loop
    
.gi_done:
    mov al, cl      ; put result in al
    
    pop cx
    mov sp, bp
    pop bp
    ret
    
; Get an int in hex format
; al = getinthex()
getinthex:
    push bp
    mov bp, sp
    push cx
    
    xor cx, cx
    mov bl, 16
.gih_loop:
    call getche
    cmp al, 13
    je .gih_done
    xor ah, ah
    sub al, '0'
    mov dx, ax
    mov ax, cx
    mul bl
    add ax, dx
    mov cx, ax
    jmp .gih_loop
    
.gih_done:
    mov al, cl
    
    pop cx
    mov sp, bp
    pop bp
    ret

; Get the mode info and store it in the buffer
; void get_cardinfo()
get_cardinfo:
    push bp
    mov bp, sp
    push ax
    push di
    push dx
    
    mov ax, ds
    mov es, ax
    
    mov ax, 4F03h       ; query current video mode
    int 10h             ; result in bx
    
    mov ax, 4F01h       ; query mode info
    mov cx, bx
    mov di, OFFSET buffer
    int 10h
    
    mov ax, [di+8h]     ; winA
    mov winA, ax
    
    xor ax, ax
    mov al, [di+19h]    ; bits per pixel
    shr ax, 3           ; bytes per pixel
    mov bpp, ax
    
    mov ax, [di+12h]    ; xRes
    mov xRes, ax
    
    mov ax, [di+14h]    ; yRes
    mov yRes, ax
    
    mov ax, [di+4h]     ; granularity
    shl ax, 10          ; in bytes
    mov gran, ax
    
    mov dx, [di+0Ch]        ; WinFuncPtr
    mov ax, [di+0Eh]
    mov WORD PTR WinFuncPtr, dx
    mov WORD PTR WinFuncPtr+2, ax
    
    pop dx
    pop di
    pop ax
    mov sp, bp
    pop bp
    ret

; Set the video mode, ah is in decimal format
; void setmode(ah : int)
setmode:
    push bp
    mov bp, sp
    push bx
    push ax
    
    mov ax, 4F02h
    mov bx, [bp+4]
    int 10h
    
    call get_cardinfo
    
    pop ax
    pop bx
    mov sp, bp
    pop bp
    ret

; Stores the color into PENCOLOR
; void setpencolor(color : int)
setpencolor:
    push bp
    mov bp, sp
    
    mov ax, [bp+4]      ; get color parameter
    mov PENCOLOR, al    ; save PENCOLOR
    
    pop bp
    ret

; Put the pen color at the provided coordinates
; void drawpixel(x : int, y : int)
drawpixel:
    push bp
    mov bp, sp
    
    push si
    push cx
    push dx
    push bx
    
    mov ax, [bp+6]  ; ax = x * bits/pixel
    mul bpp
    mov si, ax      ; si = ax
    
    mov ax, xRes    ; ax = width * bits/pixel * y
    mul bpp
    mov bx, [bp+4]
    mul bx
    add ax, si      ; ax = x * bits/pixel + width * bits/pixel * y
    adc dx, 0       ; store overflow in dx
    
    mov cx, gran    ; divide by granularity only if non-zero
    test cx, cx
    jz .dp_skip
    div cx
    xchg ax, dx     ; remainder in ax, result in dx
.dp_skip:
    mov si, ax
    
    mov ax, 4F05h   ; switch window using WinFuncPtr
    xor bx, bx
    call [DWORD PTR WinFuncPtr]
    
    mov es, winA
    
    mov ah, 0
    mov al, PENCOLOR
    mov BYTE PTR es:[si], al    ; draw the pixel
    
    pop bx
    pop dx
    pop cx
    pop si
    
    mov sp, bp
    pop bp
    ret

; Draw a line from (x1, y1) to (x2, y2) using the pen color and drawpixel
; void drawline(x1 : int, y1 : int, x2 : int, y2 : int)
drawline:
    push bp
    mov bp, sp
    
    sub sp, 6       ; make space
    
    push ax
    push bx
    push cx
    push dx
    
    mov ax, [bp+6]  ; x1
    mov bx, [bp+10] ; x0
    sub ax, bx      ; delta x
    mov [bp], ax    ; delta x: [bp]
    
    mov ax, [bp+4]  ; y1
    mov bx, [bp+8]  ; y0
    sub ax, bx      ; delta y
    mov [bp-2], ax  ; delta y: [bp-2]
    
    mov ax, [bp]    ; ax = delta x
    cmp ax, 0
    jle .dl_xpos    ; if x0 < x1
    mov cx, 1       ;   sx =  1
    jmp .dl_y       ; else
.dl_xpos:
    mov cx, -1      ;   sx = -1
    neg ax
    mov [bp], ax
    
.dl_y:
    mov ax, [bp-2]
    cmp ax, 0
    jle .dl_ypos    ; if y0 < y1
    mov dx, 1       ;   sy =  1
    neg ax          ;   negate and rewrite
    mov [bp-2], ax
    jmp .dl_next    ; else
.dl_ypos:
    mov dx, -1      ;   sy = -1
    
.dl_next:
    mov ax, [ bp ]  ; ax = delta x
    add ax, [bp-2]  ; ax -= delta y
    mov [bp-4], ax  ; error: [bp-4]
    
.dl_loop:
    push [bp+10]
    push [bp+8]
    call drawpixel
    add sp, 4
        
    mov ax, [bp+10] ; x0
    cmp [bp+6], ax  ; x1 == x0
    jne .dl_nope
    mov ax, [bp+8]  ; y0
    cmp [bp+4], ax  ; y1 == y0
    je .dl_break
.dl_nope:
        
    mov ax, [bp-4]  ; e2 = err
    shl ax, 1       ; e2 = err*2
    mov bx, [bp-2]  ; dy
    cmp ax, bx      ; if e2 > -dy
    jle .dl_skipy
    add [bp-4], bx  ; err -= dy
    add [bp+10], cx ; x0 += sx
.dl_skipy:
    mov bx, [ bp ]  ; dx
    cmp ax, bx      ; if e2 < dx
    jge .dl_skipx
    add [bp-4], bx  ; err += dx
    add [bp+8], dx  ; y0 += sy
.dl_skipx:
    jmp .dl_loop
.dl_break:
    
    pop dx
    pop cx
    pop bx
    pop ax
    mov sp, bp
    pop bp
    ret
    
; Get the color at the provided coordinates and store it in pixelColor
; int getPixelColor(x : int, y : int)
getpixelcolor:
    push bp
    mov bp, sp
    
    push di
    push si
    push cx
    push dx
    push bx
    
    mov ax, [bp+6]
    mul bpp
    mov si, ax
    
    mov ax, xRes
    mul bpp
    mov bx, [bp+4]
    mul bx
    add ax, si
    adc dx, 0
    
    mov cx, gran
    test cx, cx
    jz .pixel_skip
        div cx
        xchg ax, dx
.pixel_skip:
    mov si, ax
    
    mov ax, 4F05h
    xor bx, bx
    call [DWORD PTR WinFuncPtr]
    
    mov es, winA
    
    mov ax, es:[si]     ; store the pixel color
    mov pixelColor, al
    mov al, pixelColor
    
    pop bx
    pop dx
    pop cx
    pop si
    pop di
    
    mov sp, bp
    pop bp
    ret

; Fills the center of a shape having borders of color PENCOLOR using the FILLCOLOR
; Not optimized, very repetitive
; void simplefill(x : int, y : int)
simplefill:
    push bp
    mov bp, sp
    
    push ax
    push bx
    push cx
    push dx
    push si
    
    mov ax, [bp+6]  ; ax = initial x
    mov bx, [bp+4]  ; bx = initial y
    
.fill_loop1:
    mov si, ax              ; backup x because it will be modified by getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_loop1_exit
    
    mov ax, si
    
; positive x loop
.fill_loop1a:
    mov si, ax              ; backup x because it will be modified by getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_loop1a_exit
    
    mov cl, FILLCOLOR       ; set the pen color to the fill color
    mov PENCOLOR, cl
    
    mov ax, si              ; restore x
    
    push ax                 ; draw the pixel
    push bx
    call drawpixel
    add sp, 4
    mov PENCOLOR, dl        ; restore pen color
    
    mov ax, si              ; restore x
    inc ax
    
    jmp .fill_loop1a
    
.fill_loop1a_exit:
    
    mov ax, [bp+6]          ; restore initial x
    dec ax
    
; negative x loop
.fill_loop1b:
    mov si, ax              ; backup x because it will be modified by
                            ; getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_loop1b_exit
    
    mov cl, FILLCOLOR       ; set the pen color to the fill color
    mov PENCOLOR, cl
    
    mov ax, si              ; restore x
    
    push ax                 ; draw the pixel
    push bx
    call drawpixel
    add sp, 4
    mov PENCOLOR, dl        ; restore pen color
    
    mov ax, si              ; restore x
    dec ax
    
    jmp .fill_loop1b
    
.fill_loop1b_exit:
    mov ax, [bp+6]          ; restore initial x
    dec bx                  ; y++
    jmp .fill_loop1
    
.fill_loop1_exit:
    mov ax, [bp+6]          ; restore initial x
    mov bx, [bp+4]          ; restore initial y
    dec bx

.fill_loop2:
    mov si, ax              ; backup x because it will be modified by getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_exit
    
    mov ax, si
    
; positive x loop
.fill_loop2a:
    mov si, ax              ; backup x because it will be modified by getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_loop2a_exit
    
    mov cl, FILLCOLOR       ; set the pen color to the fill color
    mov PENCOLOR, cl
    
    mov ax, si              ; restore x
    
    push ax                 ; draw the pixel
    push bx
    call drawpixel
    add sp, 4
    mov PENCOLOR, dl        ; restore pen color
    
    mov ax, si              ; restore x
    inc ax
    
    jmp .fill_loop2a
    
.fill_loop2a_exit:
    
    mov ax, [bp+6]          ; restore initial x
    dec ax
    
; negative x loop
.fill_loop2b:
    mov si, ax              ; backup x because it will be modified by getpixelcolor and drawpixel
    push ax
    push bx
    call getpixelcolor
    add sp, 4
    
    mov cl, pixelColor      ; test for edge
    mov dl, PENCOLOR
    cmp cl, dl
    je .fill_loop2b_exit
    
    mov cl, FILLCOLOR       ; set the pen color to the fill color
    mov PENCOLOR, cl
    
    mov ax, si              ; restore x
    
    push ax                 ; draw the pixel
    push bx
    call drawpixel
    add sp, 4
    mov PENCOLOR, dl        ; restore pen color
    
    mov ax, si              ; restore x
    dec ax
    
    jmp .fill_loop2b
    
.fill_loop2b_exit:
    mov ax, [bp+6]          ; restore initial x
    inc bx                  ; y++
    jmp .fill_loop2
    
.fill_exit:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    
    mov sp, bp
    pop bp
    ret
    
END start
