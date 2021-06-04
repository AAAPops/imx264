### webcam_x264

Project that takes Raw stream from any Webcam attached to Soc i.mx6Q, convert it to h264 and send 
to another machine over Eternet. 

Probably you can use any SoC with dedicated **h264** encoder which supported by **Video4Linux** (aka v4l2)  
In SoC i.mx6Q this encoder known as **CODA960**

##### Build and run Server on i.mx6Q side:
```bash
$ mkdir arm-build && cd arm-build
$ cmake ../
$ make

$ ./webcam_x264 -d /dev/video2 -P 5100 -c 0 -D 2
```

##### Build and run proxy-client on x86 side:
```bash
$ mkdir x86-build && cd x86-build
$ cmake ../proxy-client/
$ make

$ ./v-client -w 640 -h 480 -f 25 -D2 -S 10.1.91.123:5100 |  gst-launch-1.0 -v fdsrc fd=0 ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
```

**v-client** output pure h264 stream to STDOUT. You can use it as you wish.      
In the example above I use Gstreamer to show result on the screen.

In ***sync-frames*** branch you can use buffered **v-client** that makes  *h264 stream* more fluent but add some delay which depends on queue length.

------

##### The same in Russian

Проект, позволяющий взять "сырой" поток с бытовой Web-камеры, перекодировать его в h264 поток и выдать в сеть.  
В качестве платформы для кодирования необходимо использовать процессор **i.mx6**

Чтобы собрать и запустить сервер на ТС-50:
```bash
$ mkdir arm-build && cd arm-build
$ cmake ../
$ make

$ ./webcam_x264 -d /dev/video2 -P 5100 -c 0 -D 2
```

Чтобы собрать и запустить прокси-клиента на x86:
```bash
$ mkdir x86-build && cd x86-build
$ cmake ../proxy-client/
$ make

$ ./v-client -w 640 -h 480 -f 25 -D2 -S 10.1.91.123:5100 |  gst-launch-1.0 -v fdsrc fd=0 ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
```

***Значение всех опций есть в описании к каждой утилите, достаточно запустить оную без параметров***
