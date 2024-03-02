 #include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

size_t Buffer::ReadableBytes() const {  // 可以读的数据的大小
    return writePos_ - readPos_;
}
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 前面可以用的空间
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

//buff.RetrieveUntil(lineEnd + 2);
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

//  Append(buff, len - writable);   buff临时数组，len-writable是临时数组中的数据个数
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);                       //对缓冲区进行扩容，len是缓冲区溢出的数据大小
    std::copy(str, str + len, BeginWrite());    //将str 到 str+len位置的数据复制到BeginWrite()位置
    HasWritten(len);                            //修改当前写的位置
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);                    //对缓冲区进行扩容
    }
    assert(WritableBytes() >= len);
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    
    char buff[65535];   // 临时的数组，保证能够把所有的数据都读出来

    //iovec 用于描述输入/输出向量。它主要在UNIX-like系统的I/O操作中使用，尤其是与readv和writev函数一起使用。
    struct iovec iov[2];    //这些函数允许程序一次处理多个缓冲区，而不是一个接一个地处理，从而提高了I/O效率
    const size_t writable = WritableBytes();    //剩余可写数据的缓冲区大小
    
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;  //指向要读或写数据的缓冲区的指针。地址
    iov[0].iov_len = writable;                  //该缓冲区中要读或写的字节数。
    iov[1].iov_base = buff;                     //指向要读或写数据的缓冲区的指针。地址
    iov[1].iov_len = sizeof(buff);              //该缓冲区中要读或写的字节数。

    const ssize_t len = readv(fd, iov, 2);  //readv或writev时，可以传递一个iovec结构体数组，可以一次性读或写多个缓冲区。返会读到的数据长度
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) { //如果读到的数据大小小于剩余缓冲区的大小
        writePos_ += len;                           //目前写到的缓冲位置加上读到的数据长度
    }
    else {                                          //如果读到的数据大小大于剩余缓冲区大小
        writePos_ = buffer_.size();                 //目前写到的缓冲区位置设置到缓冲区尾部后一位
        Append(buff, len - writable);               //
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

//对缓冲区进行扩容
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {    //当前缓冲区剩余可写的空间 + 当前缓冲区读到的位置 < 剩余需要接受的数据大小
        buffer_.resize(writePos_ + len + 1);            //重新指定缓冲区大小为：当前写到的位置 + 剩余需要接受的数据长度 + 1
    } 
    else {
        size_t readable = ReadableBytes();              //当前缓冲区剩余可读的空间
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());    //将当前读到的位置到当前写到的位置之间的数据拷贝到缓冲区起始位置."00(11122)" 到 "(11122)22"
        readPos_ = 0;
        writePos_ = readPos_ + readable;                //当前写的位置 = 当前读的位置 + 当前剩余可读的空间
        assert(readable == ReadableBytes());
    }
}