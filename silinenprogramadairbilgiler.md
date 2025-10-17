src/main.cpp:664 Bellek taraması chunk’ları ardışık ama örtüşmesiz okuyor; desenin son birkaç baytı bir blokta, ilk baytı sonraki blokta kaldığında sonuç tamamen kaçıyor (özellikle kısa string ve 4 baytlık int aramalarda öğretmen bunu kolay fark eder).
src/main.cpp:159 UTF‑16 dönüşümü std::wstring(text.begin(), text.end()) ile yapıldığı için Türkçe karakterlerde (ö, ş vb.) arama ve yazma bozuluyor; aynı problem Utf16BytesToString içinde de ters yönde mevcut.
src/memory/MemoryUtils.cpp:22 Tüm ReadProcessMemory çağrılarında bytesRead == size şartı konmuş; remote sayfanın sadece bir kısmı okunabildiğinde iç döngü break edip aynı bölgedeki kalan sayfaları taramıyor, bu da değerleri gözden kaçırma riski yaratıyor.
src/main.cpp:594 Arayüz yalnızca 32 bit signed integer ve sabit uzunluklu ASCII/UTF‑16 string destekliyor; değeri değiştirmek için aynı uzunluk zorunluluğu var ve ardıl “next scan” süreci bulunmuyor, bu da tipik bellek aracı beklentilerini karşılamıyor.
src/memory/StealthMemoryUtils.cpp:246 ScanPatternStealth için sürücü yoksa geri dönüş tamamen boş; README’de vaat edilen pattern taraması pratikte normal API ile hiç yok.
Programın menüsü, logging’i ve stealth ayarları sunumu hoş, fakat yukarıdaki eksikler giderilmeden hocaya sunmak riskli olur; en azından UTF‑16/çok baytlı destek ve tarama doğruluğu tamamlanmalı. İyileştirmelerden sonra genişletilmiş veri tipi seçenekleri ve art arda filtreleme (next scan) eklemek projeyi çok daha ikna edici hale getirir.

tamam yukarıda ki düzenlemeleri yap ve aşağıdaki

Gelişmiş bir eğitim ve sistem analiz aracı geliştiriyorum.
Amaç, Windows üzerinde process’lerin ve bellek bölgelerinin güvenli ve kontrollü şekilde izlenmesi, analiz edilmesi ve gerektiğinde eğitimsel amaçlarla veri takibinin sağlanmasıdır.

Yapılacak Geliştirmeler:

Değer Takibi ve Canlı İzleme

Kullanıcı belirli bir değeri (sayı/string) girer.

Program, sistem belleğinde bu değerin geçtiği tüm adresleri bulup numaralandırarak gösterir.

Sonrasında kullanıcı isterse tek tek, isterse toplu olarak seçili adreslerde değer değişimini canlı izleyebilir.

Canlı izleme (watch) özelliği ile seçili adres(ler)deki değişimler periyodik olarak ekranda/log’da gösterilsin.

İncremental/Refine Tarama

Bulunan adresler üzerinden, değeri tekrar güncelleyerek daraltma (refine/next scan) yapılabilsin.

Böylece sadece gerçek zamanlı değişen/değiştirilen veri adresleri kolayca tespit edilebilsin.

Sade ve Sezgisel Arayüz

Kullanıcı deneyimi sade, hızlı ve pratik olmalı.

Menüde gereksiz seçenekler, teknik karmaşa veya fazla alt menü olmamalı.

Sık yapılan işlemler için kısa komut desteği sağlanmalı (örn. “r 3” ile 3. adresi oku, “w 2 1234” ile 2. adrese yeni değer yaz).

Veri Güncelleme ve Sabitleme

Kullanıcı dilerse, seçili adres(ler)deki değeri güvenli şekilde güncelleyebilir veya sabitleyebilir (freeze).

İşlemlerin tamamı güvenli, etik ve kontrol edilebilir olmalı.

Güvenlik ve Sistem Uyumluluğu

Kernel seviyesinde (isteğe bağlı) destek eklenebilir; gerçek bir kernel-mode sürücü ile, modern Windows sistemlerinde güvenli erişim sağlanmalı.

Kullanıcı modu ile sürücü arasında güvenli iletişim (IPC/IOCTL) best practice’lerine uygun geliştirilmeli.

Driver imzalama, yükleme ve güncel güvenlik gereksinimlerine tamamen uyumlu olmalı.

Dokümantasyon ve Loglama

Tüm yeni özellikler detaylıca dökümante edilmeli.

Gerçekleşen işlemler sade ve anlaşılır bir şekilde log’lanmalı.


gömülü gerçek bir kernel-mode sürücüsü yapmayı unutma !


Swapped the old menu for a command-driven console with refine/next-scan, read/write shortcuts, and watch/freeze management (src/main.cpp:1310, src/main.cpp:1636), including helper utilities for encoding, command parsing, and status output.
Added live watch / freeze infrastructure—shared helpers (src/main.cpp:875), freeze creation (src/main.cpp:996), and the polling loop that enforces freezes and reports changes in real time (src/main.cpp:1165).
Implemented the kernel-mode companion driver handling IOCTL-based read/write/protect (src/driver/MemoryPatcherDrv.c:33) and taught the user-mode bridge to auto-install/open it while falling back gracefully when unavailable (src/kernel/StealthDriver.cpp:37, src/kernel/StealthDriver.cpp:354, src/kernel/StealthDriver.cpp:423).
Added a dedicated driver build script (build_driver.bat) and refreshed the README with the new workflow, command palette, and driver instructions (README.md).
Rebuild the console with build.bat, then (from a WDK prompt) run build_driver.bat so MemoryPatcherDrv.sys sits beside the EXE.
Launch MemoryPatcher.exe, run help, and walk through attach, search, refine, watch, and freeze to validate the new console flow and live monitoring.
Keep an eye on logs/ to confirm that driver loading, watch updates, and freeze enforcement are being recorded as expected.
==========================================
MemoryPatcher - Build Script
[*] Compiling sources...
main.cpp
C:\Users\pc\Desktop\TSH\src\main.cpp(430): error C3861: 'ReadProcessMemorySafe': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\main.cpp(436): error C3861: 'WriteProcessMemorySafe': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\main.cpp(451): error C3861: 'ReadProcessMemorySafe': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\main.cpp(1369): error C3861: 'PerformSearch': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\main.cpp(1382): error C3861: 'RefineSearchResults': tanımlayıcı bulunamadı
MemoryUtils.cpp
AOBScanner.cpp
StealthMemoryUtils.cpp
StealthDriver.cpp
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(175): error C2065: 'FILE_DEVICE_UNKNOWN': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(175): error C2065: 'METHOD_BUFFERED': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(175): error C2065: 'FILE_ANY_ACCESS': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(175): error C3861: 'CTL_CODE': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(222): error C2065: 'FILE_DEVICE_UNKNOWN': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(222): error C2065: 'METHOD_BUFFERED': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(222): error C2065: 'FILE_ANY_ACCESS': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(222): error C3861: 'CTL_CODE': tanımlayıcı bulunamadı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(260): error C2065: 'FILE_DEVICE_UNKNOWN': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(260): error C2065: 'METHOD_BUFFERED': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(260): error C2065: 'FILE_ANY_ACCESS': bildirimi yapılmamış tanımlayıcı
C:\Users\pc\Desktop\TSH\src\kernel\StealthDriver.cpp(260): error C3861: 'CTL_CODE': tanımlayıcı bulunamadı
Syscalls.cpp
ManualMapper.cpp
AntiDebug.cpp


Added forward declarations for the memory helper and search functions so they’re visible before MonitorLoop/ProcessCommand use them (src/main.cpp:100). This resolves the C3861 “identifier not found” build errors for ReadProcessMemorySafe, WriteProcessMemorySafe, PerformSearch, and RefineSearchResults.
Included the correct Windows IOCTL headers in the shared driver interface (src/kernel/DriverIoctl.h:6), letting the user‑mode build see FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, FILE_ANY_ACCESS, and CTL_CODE.
Extended the stealth driver bridge to manage driver handles and fall back cleanly, adding device/driver helpers plus the necessary includes (src/kernel/StealthDriver.h:8, src/kernel/StealthDriver.cpp:1, src/kernel/StealthDriver.cpp:37, src/kernel/StealthDriver.cpp:159), and removed duplicate prototypes in main.cpp (src/main.cpp:559).
Re-run build.bat to rebuild the console; the missing identifier and IOCTL macro errors should be gone.
If you plan to use kernel assistance, build the driver from a WDK prompt with build_driver.bat and copy MemoryPatcherDrv.sys next to the EXE (already reflected in the docs).




======================================== MemoryPatcher [Stealth] ======================================== No process attached. ---------------------------------------- Commands: help : Show this help summary status : Show attachment and monitoring state attach [pid] : Attach to a process (list chooser if pid omitted) detach : Detach from current process search <type> <value> : Scan memory (type: i, ascii, utf16) refine <value> : Filter last results with new value results [count] : Display last matches (default configured limit) read <indices|*> : Read value(s) from matched addresses write <indices|*> <value> : Write value to matched addresses watch add <indices|*> : Add addresses to live watch list watch remove <indices|*> : Remove watch entries watch list : Show active watches freeze add <indices|*> [value] : Freeze addresses (optionally with new value) freeze remove <indices|*> : Remove freeze entries freeze list : Show active freezes freeze clear : Clear all freeze entries interval <ms> : Set watch/freeze polling interval (default 500) settings : Show current settings settings stealth <on|off> : Toggle stealth mode settings antidebug <on|off>: Toggle anti-debug subsystem settings terminate <on|off>: Toggle terminate-on-detection maxresults <count> : Limit displayed matches when using results command quit / exit : Terminate MemoryPatcher MemoryPatcher interactive console. Type 'help' for commands. mp> attach Available processes: ---------------------------------------- [ 1] 0 [System Process] [ 2] 4540 AdobeUpdateService.exe [ 3] 5416 AdskAccessServiceHost.exe [ 4] 5316 AdskLicensingService.exe [ 5] 8980 AnyDesk.exe [ 6] 15280 AnyDesk.exe [ 7] 4004 ApplicationFrameHost.exe [ 8] 1900 ASCService.exe [ 9] 17064 audiodg.exe [ 10] 20368 backgroundTaskHost.exe [ 11] 9108 CCleaner.exe [ 12] 5172 CCleaner_service.exe [ 13] 5220 cer_service.exe [ 14] 2272 chrome.exe [ 15] 3316 chrome.exe [ 16] 3808 chrome.exe [ 17] 5800 chrome.exe [ 18] 5884 chrome.exe [ 19] 6868 chrome.exe [ 20] 7188 chrome.exe [ 21] 8992 chrome.exe [ 22] 10600 chrome.exe [ 23] 11928 chrome.exe [ 24] 12528 chrome.exe [ 25] 13340 chrome.exe [ 26] 13708 chrome.exe [ 27] 14344 chrome.exe [ 28] 15856 chrome.exe [ 29] 16024 chrome.exe [ 30] 17112 chrome.exe [ 31] 19448 chrome.exe [ 32] 20652 chrome.exe [ 33] 20868 chrome.exe [ 34] 21656 chrome.exe [ 35] 22244 chrome.exe [ 36] 13692 cloudcode_cli.exe [ 37] 5512 Code.exe [ 38] 6400 Code.exe [ 39] 8608 Code.exe [ 40] 10260 Code.exe [ 41] 15488 Code.exe [ 42] 16040 Code.exe [ 43] 17876 Code.exe [ 44] 17944 Code.exe [ 45] 18700 Code.exe [ 46] 19220 Code.exe [ 47] 20572 Code.exe [ 48] 21240 Code.exe [ 49] 21376 Code.exe [ 50] 16964 CodeSetup-stable-7d842fb85a0275a4a8e4d7e040d2625abbf7f084.exe [ 51] 4288 CodeSetup-stable-7d842fb85a0275a4a8e4d7e040d2625abbf7f084.tmp [ 52] 12744 codex.exe [ 53] 5480 conhost.exe [ 54] 9248 conhost.exe [ 55] 11064 conhost.exe [ 56] 15520 conhost.exe [ 57] 17936 conhost.exe [ 58] 18296 conhost.exe [ 59] 18920 conhost.exe [ 60] 19268 conhost.exe [ 61] 19704 conhost.exe [ 62] 20276 conhost.exe [ 63] 22508 conhost.exe [ 64] 980 csrss.exe [ 65] 1064 csrss.exe [ 66] 13068 ctfmon.exe [ 67] 2368 Cursor.exe [ 68] 7004 Cursor.exe [ 69] 8708 Cursor.exe [ 70] 14728 Cursor.exe [ 71] 17312 Cursor.exe [ 72] 17968 Cursor.exe [ 73] 18732 Cursor.exe [ 74] 18740 Cursor.exe [ 75] 18752 Cursor.exe [ 76] 19888 Cursor.exe [ 77] 19920 Cursor.exe [ 78] 21904 Cursor.exe [ 79] 22396 Cursor.exe [ 80] 22412 Cursor.exe [ 81] 5696 dasHost.exe [ 82] 15676 DataExchangeHost.exe [ 83] 11516 dllhost.exe [ 84] 11712 dllhost.exe [ 85] 15792 dllhost.exe [ 86] 1832 dwm.exe [ 87] 18868 EdgeGameAssist.exe [ 88] 9444 explorer.exe [ 89] 4604 FNPLicensingService64.exe [ 90] 1388 fontdrvhost.exe [ 91] 1396 fontdrvhost.exe [ 92] 23068 GameBar.exe [ 93] 11624 GameBarFTServer.exe [ 94] 16596 GameBarPresenceWriter.exe [ 95] 14808 GameInputSvc.exe [ 96] 14820 GameInputSvc.exe [ 97] 14400 gamingservices.exe [ 98] 14452 gamingservicesnet.exe [ 99] 5424 GigabyteUpdateService.exe [100] 9028 League of Legends.exe [101] 15308 LeagueClient.exe [102] 13212 LeagueClientUx.exe [103] 9856 LeagueClientUxRender.exe [104] 13364 LeagueClientUxRender.exe [105] 14376 LeagueClientUxRender.exe [106] 17300 LeagueClientUxRender.exe [107] 17604 LeagueClientUxRender.exe [108] 18368 LeagueClientUxRender.exe [109] 16216 LeagueCrashHandler64.exe [110] 16696 LeagueCrashHandler64.exe [111] 10624 LocationNotificationWindows.exe [112] 2504 logi_lamparray_service.exe [113] 1152 LsaIso.exe [114] 1168 lsass.exe [115] 3384 Memory Compression [116] 16992 MemoryPatcher.exe [117] 9012 Monitor.exe [118] 5124 MpDefenderCoreService.exe [119] 296 msedge.exe [120] 1572 msedge.exe [121] 5664 msedge.exe [122] 11312 msedge.exe [123] 15652 msedge.exe [124] 19644 msedge.exe [125] 20564 msedge.exe [126] 22436 msedge.exe [127] 23304 msedge.exe [128] 5448 MsMpEng.exe [129] 7564 NisSrv.exe [130] 2336 Notepad.exe [131] 3156 NVDisplay.Container.exe [132] 3728 NVDisplay.Container.exe [133] 15076 OneDrive.Sync.Service.exe [134] 18832 OpenConsole.exe [135] 17664 PhoneExperienceHost.exe [136] 2056 powershell.exe [137] 6672 powershell.exe [138] 12048 powershell.exe [139] 14992 powershell.exe [140] 15616 powershell.exe [141] 19836 powershell.exe [142] 20128 powershell.exe [143] 16188 python.exe [144] 180 Registry [145] 14020 RiotClientCrashHandler.exe [146] 13416 RiotClientServices.exe [147] 2068 RuntimeBroker.exe [148] 3552 RuntimeBroker.exe [149] 10712 RuntimeBroker.exe [150] 11188 RuntimeBroker.exe [151] 11952 RuntimeBroker.exe [152] 15596 RuntimeBroker.exe [153] 19764 SearchFilterHost.exe [154] 23440 SearchFilterHost.exe [155] 10432 SearchHost.exe [156] 13304 SearchIndexer.exe [157] 12272 SearchProtocolHost.exe [158] 140 Secure System [159] 13736 SecurityHealthService.exe [160] 13748 SecurityHealthSystray.exe [161] 1132 services.exe [162] 5012 ShellExperienceHost.exe [163] 8804 sihost.exe [164] 15420 smartscreen.exe [165] 668 smss.exe [166] 4736 spoolsv.exe [167] 5304 sqlbrowser.exe [168] 2584 sqlceip.exe [169] 5188 sqlservr.exe [170] 5328 sqlwriter.exe [171] 10456 StartMenuExperienceHost.exe [172] 1312 svchost.exe [173] 1360 svchost.exe [174] 1496 svchost.exe [175] 1544 svchost.exe [176] 1624 svchost.exe [177] 1648 svchost.exe [178] 1680 svchost.exe [179] 1688 svchost.exe [180] 1744 svchost.exe [181] 1788 svchost.exe [182] 1880 svchost.exe [183] 1892 svchost.exe [184] 2012 svchost.exe [185] 2156 svchost.exe [186] 2164 svchost.exe [187] 2360 svchost.exe [188] 2380 svchost.exe [189] 2512 svchost.exe [190] 2520 svchost.exe [191] 2628 svchost.exe [192] 2660 svchost.exe [193] 2696 svchost.exe [194] 2732 svchost.exe [195] 2784 svchost.exe [196] 2804 svchost.exe [197] 2856 svchost.exe [198] 3100 svchost.exe [199] 3196 svchost.exe [200] 3228 svchost.exe [201] 3236 svchost.exe [202] 3244 svchost.exe [203] 3324 svchost.exe [204] 3352 svchost.exe [205] 3360 svchost.exe [206] 3652 svchost.exe [207] 3708 svchost.exe [208] 3916 svchost.exe [209] 3924 svchost.exe [210] 3992 svchost.exe [211] 4120 svchost.exe [212] 4216 svchost.exe [213] 4672 svchost.exe [214] 4748 svchost.exe [215] 4972 svchost.exe [216] 5068 svchost.exe [217] 5084 svchost.exe [218] 5092 svchost.exe [219] 5100 svchost.exe [220] 5232 svchost.exe [221] 5280 svchost.exe [222] 5432 svchost.exe [223] 5440 svchost.exe [224] 6140 svchost.exe [225] 6392 svchost.exe [226] 6992 svchost.exe [227] 7096 svchost.exe [228] 7688 svchost.exe [229] 7780 svchost.exe [230] 7952 svchost.exe [231] 8036 svchost.exe [232] 8092 svchost.exe [233] 8120 svchost.exe [234] 8852 svchost.exe [235] 8884 svchost.exe [236] 8920 svchost.exe [237] 9260 svchost.exe [238] 9452 svchost.exe [239] 9868 svchost.exe [240] 9964 svchost.exe [241] 10032 svchost.exe [242] 10248 svchost.exe [243] 10328 svchost.exe [244] 11052 svchost.exe [245] 11356 svchost.exe [246] 12900 svchost.exe [247] 13328 svchost.exe [248] 13396 svchost.exe [249] 13932 svchost.exe [250] 14504 svchost.exe [251] 20072 svchost.exe [252] 20080 svchost.exe [253] 20308 svchost.exe [254] 23268 svchost.exe [255] 4 System [256] 3404 SystemSettings.exe [257] 21664 SystemSettingsBroker.exe [258] 8984 taskhostw.exe [259] 13024 TextInputHost.exe [260] 13940 UninstallMonitor.exe [261] 11396 UserOOBEBroker.exe [262] 18240 vctip.exe [263] 18280 vgc.exe [264] 13992 vgtray.exe [265] 4456 vmcompute.exe [266] 3032 vmms.exe [267] 12644 WhatsApp.exe [268] 11168 Widgets.exe [269] 11488 WidgetService.exe [270] 23104 WindowsTerminal.exe [271] 1056 wininit.exe [272] 1236 winlogon.exe [273] 12592 WmiPrvSE.exe [274] 22972 WmiPrvSE.exe [275] 5468 wslservice.exe [276] 1192 WUDFHost.exe [277] 15848 XboxPcAppFT.exe Select index or PID (q to cancel): 130 Process attached successfully. mp> mp> search utf16 butesttir Scan completed. 15 matches found. [1] 0x299806B8096 -> "butesttir" (UTF-16) [2] 0x2998AB48C66 -> "butesttir" (UTF-16) [3] 0x2998AB694E2 -> "butesttir" (UTF-16) [4] 0x2998ABA3AF0 -> "butesttir" (UTF-16) [5] 0x2998B68A780 -> "butesttir" (UTF-16) [6] 0x2998B68A840 -> "butesttir" (UTF-16) [7] 0x2998B6D8910 -> "butesttir" (UTF-16) [8] 0x2998B6F65C8 -> "butesttir" (UTF-16) [9] 0x2998DFA40DA -> "butesttir" (UTF-16) [10] 0x2998E2ECBF2 -> "butesttir" (UTF-16) [11] 0x2998E2ED676 -> "butesttir" (UTF-16) [12] 0x2998E3841BE -> "butesttir" (UTF-16) [13] 0x2998E3C24B6 -> "butesttir" (UTF-16) [14] 0x2998EA16456 -> "butesttir" (UTF-16) [15] 0x7FFF89AA15B2 -> "butesttir" (UTF-16) mp> results [1] 0x299806B8096 -> "butesttir" (UTF-16) [2] 0x2998AB48C66 -> "butesttir" (UTF-16) [3] 0x2998AB694E2 -> "butesttir" (UTF-16) [4] 0x2998ABA3AF0 -> "butesttir" (UTF-16) [5] 0x2998B68A780 -> "butesttir" (UTF-16) [6] 0x2998B68A840 -> "butesttir" (UTF-16) [7] 0x2998B6D8910 -> "butesttir" (UTF-16) [8] 0x2998B6F65C8 -> "butesttir" (UTF-16) [9] 0x2998DFA40DA -> "butesttir" (UTF-16) [10] 0x2998E2ECBF2 -> "butesttir" (UTF-16) [11] 0x2998E2ED676 -> "butesttir" (UTF-16) [12] 0x2998E3841BE -> "butesttir" (UTF-16) [13] 0x2998E3C24B6 -> "butesttir" (UTF-16) [14] 0x2998EA16456 -> "butesttir" (UTF-16) [15] 0x7FFF89AA15B2 -> "butesttir" (UTF-16) mp>