# BannerBar

This software will display the classification level banner in screen with Windows AppBar.

For NetSync feature now is in development. It use for sync banner from server.

<img width="1918" height="1079" alt="image" src="https://github.com/user-attachments/assets/95adea52-fee0-4600-977d-4c68f9dbd149" />

For compile release please use
```bash
g++ main.cpp -O2 -static -luser32 -lgdi32 -lshell32 -ladvapi32 -lws2_32 -liphlpapi -lpsapi -Wl,-subsystem,windows -o main.exe
```
