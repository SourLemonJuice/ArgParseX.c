#!/usr/bin/env bash

./test.out param1 \
    --test=testStr1,testStr2 \
    param2 \
    --setbool \
    param3 \
    -baa \
    -a \
    ++test2~str1-str2 \
    /win1Param1/win2Param2 \
    --paramlist=a,b,c,d,e,f \
    --setint \
    paramEnd
