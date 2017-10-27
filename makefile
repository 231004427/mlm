CC=gcc
TARGET=mlm
#编译选项
CFLAGS=-Wall -g
#宏定义-D__LINUX__
DEFS = -D__DEBUG__ 
CFLAGS += $(DEFS)
# 头文件查找路径  
INC = -I./ -L/usr/local/lib -levent -lpthread -llog4c -lmysqlclient
#源文件夹
DIRS=.
FILES=$(foreach dir,$(DIRS),$(wildcard $(dir)/*.c))
OBJS=$(patsubst %.c,%.o,$(FILES))
RM=rm -f
.SUFFIXES:.c .o
.c.o:
	$(CC) -c -o $@ $^ $(CFLAGS)
$(TARGET):$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS) $(INC)

clean:
	-$(RM) $(TARGET)
	-$(RM) $(OBJS) 
	-$(RM) *.o
	-$(RM) *.out
