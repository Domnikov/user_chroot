# user_chroot

Found this source in the internet and modified it for my purpose.


Initially was only start bash. I need to start scripts after connect on ssh settings in jail environment.


compilation: 

g++ -o user_chroot ./user_chroot.cpp

using:


    start user bash in jail: ./user_chroot

    executing commands ./user_chroot
    
    

example: 

./user_chroot . /bin/bash -c 'cd ~/src;g++ -c ./*.cpp' - compile all cpp files in jail in home/src
