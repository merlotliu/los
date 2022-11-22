if [[ ! -d "../lib" || ! -d "../build" ]];then
    echo "dependent dir don\'t exist!"
    cwd=$(pwd)
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [[ $cwd != "command" ]];then
        echo -e "you\'d better in command dir\n"
    fi
    exit
fi

BIN="proc"
CFLAGS="-m32 -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIB= ""# "-I ../kernel/ -I ../lib/ -I ../lib/kernel -I ../lib/user -I ../thread -I ../userprog -I ../fs -I ../device -I ../thread -I ../shell"
OBJS=""#"./build/*.o"
DD_IN=$BIN
DD_OUT="/home/ml/bochs/hd60M.img"

gcc $CFLAGS $LIB -o $BIN".o" $BIN".c"
# ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN -Ttext 0x9050000
ld -m elf_i386 -e main $BIN".o" -o $BIN -Ttext 0x2000000
SEC_CNT=$(ls -l $BIN|awk '{printf ("%d", ($5+511)/512) }')

if  [[  -f$BIN  ]];then
    dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi
#################
#gcc -wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
# -wsystem-headers -I ../1ib -0 prog_ _no_ _arg.o prog_ no_ _arg.c
#ld -e main prog_ no_ arg.o●./build/string.o . . /build/syscall.o\
# . . /build/stdio.o . . /build/assert.o -0 prog_ _no_ arg
#dd if=prog_ no_ arg of= /home / work/my_ workspace/bochs/hd60M.img bs=512 count=10 seek=300 conv=notrunc
