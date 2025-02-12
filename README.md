# Ugrađeni računarski sistemi 2024 ETFBL izvještaj 

## Zadatak
**Tema 5**

Obezbijediti podršku za kontrolu radio prijemnika (FM Click dodatna pločica sa integrisanim kolom Si4703). Napraviti jednostavnu aplikaciju koja iz komandne linije korisniku omogućava manipulaciju ovim radio prijemnikom.


## Konfiguracija sistema
Za potrebe izrade projektnog zadatka korišten je manuelni pristup konfiguracije iz laboratorijskih vježbi 1, 4, 5 i 6.

## Dodatna konfiguracija

Patch de1-soc-handoff.patch je primjenjen ulaskom u u-boot direktorijum i pokretanjem komande 
    
    git apply de1-soc-handoff.patch

Varijable iz boot-env.txt su dodane ručno pomoću setenv, editenv i saveenv komandi.

Fajl socfpga.rbf se kopira na FAT32 particiju SD kartice.

Pošto si470x driver radi sa interrupt-ima potrebno je dodati sljedeće u device tree.

    gpio_fpga: gpio@ff200000 {
        interrupt-parent = <&intc>;
        compatible = "altr,pio-1.0";
        reg = <0xff200000 0x10>;
        interrupts = <0 40 3>;
        altr,ngpio = <16>;
        altr,interrupt-type = <IRQ_TYPE_EDGE_FALLING>;
        #gpio-cells = <2>;
        gpio-controller;
        #interrupt-cells = <2>;
        interrupt-controller;
    };

## Dodavanje driver-a

U kernel konfiguraciji pod Device Drivers pronađemo i uključimo stavku Multimedia Support.
Zatim pod stavkom Multimedia Support uključimo 
* Media core support -> Video4Linux core i
* Media drivers -> Radio adapters -> Silicon Labs Si470x FM Radio Receiver support (i Silicon Labs Si470x FM Radio Receiver support with I2C).

## Device tree
Primjer device tree konfiguracije za uređaj je moguće naći u linux stablu pod Documentation/devicetree/bindings/media/i2c/si470x.txt.

Potrebno je promijeniti i2c adresu na 0x10 i postaviti odgovarajuće GPIO pinove.

    &i2c2 {
        status = "okay";

        si4703@10 {
            compatible = "silabs,si470x";
            reg = <0x10>;

            interrupt-parent = <&gpio_fpga>;
            interrupts = <0 IRQ_TYPE_EDGE_FALLING>;
            reset-gpios = <&gpio_fpga 1 GPIO_ACTIVE_HIGH>;
        };
    };

Na GPIO0 povežemo GP2 pin FM Click pločice, a na GPIO1 RST pin.

## v4l-utils
Za upravljanje uređajem možemo koristiti v4l-utils alate. 

Kompajlirani su sljedećim komandama.

    cd v4l-utils-1.24.1/
    ./bootstrap.sh 
    export PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig
    CC=arm-linux-gcc PKG_CONFIG="pkg-config --static" LDFLAGS="--static -static" ./configure --host=arm-linux --prefix=/usr --disable-shared
    make
    make install DESTDIR=~/rootfs

## Pokretanje
Nakon uključivanja ploče, ako smo dodali driver kao modul možemo ga učitati sa komandom

    modprobe i2c:si470x

i dobijamo ispis

    [   11.750068] mc: Linux media interface: v0.10
    [   11.861615] videodev: Linux video capture interface: v2.00
    [   12.017001] si470x 1-0010: DeviceID=0x1242 ChipID=0x1200
    [   12.022306] si470x 1-0010: This driver is known to work with firmware version 12, but the device has firmware version 0.
    [   12.022306] If you have some trouble using this driver, please report to V4L ML at linux-media@vger.kernel.org

Sada bi se uređaj trebao pojaviti pod /dev/radio0.

Da bismo testirali uređaj možemo iskoristiti komandu

    v4l2-ctl -d /dev/radio0 --set-ctrl=volume=10,mute=0 --set-freq=95.21 --all

Međutim dobijamo grešku

    [  940.095821] video4linux radio0: tune does not complete
    [  940.100955] video4linux radio0: tune timed out after 3000 ms

## Problem
Ispostavlja se da tune komanda nikad ne završi ako su interrupt-i u uključeni na uređaju (STCIEN bit).

Primjeri na internetu sa istim problemom:

https://community.nxp.com/t5/i-MX-Processors/V4L2-radio-tuner-si4702/m-p/1233619

https://community.silabs.com/s/question/0D51M00007xeOkHSAU/si4703-cannot-enable-stc-seek-tunecomplete-interrupt?language=it

## Rješenje

Nije neophodno koristiti interrupt-e za komunikaciju sa uređajem, možda i da se radi polling.

U tu svrhu korištena je biblioteka https://github.com/cmumford/si470x.

Sada RST pin povezujemo na dodatni 3.3V pin da bismo uključili FM Click pločicu.

## Kompajliranje biblioteke

    cd si470x
    mkdir -p build && cd build
    cmake .. -DCMAKE_C_COMPILER=$HOME/x-tools/arm-etfbl-linux-gnueabihf/bin/arm-linux-gcc
    cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .

## Kompajliranje primjera

Uz biblioteku autor je dostavio i primjere korištenja. 

Jedan primjer (si470x_examples-master/example/unix/rdsdisplay.cc) je prilagođen za potrebe projekta.

Kompajlira se na sličan način:

    cd si470x_examples-master
    mkdir -p build && cd build
    cmake .. -DCMAKE_C_COMPILER=$HOME/x-tools/arm-etfbl-linux-gnueabihf/bin/arm-linux-gcc
    cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .

Takođe, potrebno je na isti način kompajlirati i biblioteku u rds folderu prije toga.

Sada prebacimo izvršni fajl rdsdisplay na ploču.

## Pokretanje

Pokretanjem sa ./rdsdiplay dobijamo meni

    [t] Tune to given frequency
    [u] Seek up
    [d] Seek down
    [v] Volume control
    [q] Quit

 a automatski je puštena stanica 94.4Mhz.
