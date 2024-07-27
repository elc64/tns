RC=brcc32
RCFLAGS=-32

CC=bcc32
CFLAGS=-c -tWC -IC:\Borland\BCC55\Include

LD=ilink32
LDFLAGS=-ap -c -x -Gn -LC:\Borland\BCC55\Lib
OBJLIB=c0x32.obj
LIBS=import32.lib cw32.lib

SOURCE=tns
TARGET=tns

OBJS=$(SOURCE).obj

$(TARGET).exe: $(OBJS) app.def $(TARGET).res
 $(LD) $(LDFLAGS) $(OBJLIB) $(OBJS), $(TARGET),, $(LIBS), app.def, $(TARGET).res

$(TARGET).res: $(TARGET).rc
	$(RC) $(RCFLAGS) $(TARGET).rc
