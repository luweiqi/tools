#!/bin/bash

succ_count=0
fail_count=0

#target_image=/home/pi/oss/nanoDAP/firmware/v2.3/stm32f103c8t6/flash_image_cmsisdap_20191007.bin
target_image=/home/pi/oss/nanoDAP/firmware/v2.3/DAPLink/flash_image_128k_bl_if_hid.bin

while [ 1 ]; do
    ./probe.sh
    if [ $? -eq 0 ]; then

        ./flash_write.sh ${target_image}
            
        if [ $? -eq 0 ]; then
            succ_count=$(($succ_count+1))  
            echo "---------- SUCC [$succ_count] ----------"
        else
            fail_count=$(($fail_count+1))  
            echo "---------- FAIL [$fail_count] ----------"
        fi

        while [ 1 ]; do
            ./probe.sh
            if [ $? -eq 1 ]; then
                break;
            else
                echo "wait detach..."
                sleep 0.1
            fi
        done

    else
        echo "wait attach..."
        sleep 0.1

    fi

done