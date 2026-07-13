    .section .rodata.cloudflared,"a",@progbits
    .balign 16
    .global cloudflared_blob_start
    .global cloudflared_blob_end
cloudflared_blob_start:
    .incbin "assets/cloudflared-linux-arm64"
cloudflared_blob_end:
    .section .note.GNU-stack,"",@progbits