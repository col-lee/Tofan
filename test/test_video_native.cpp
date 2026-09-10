#include "../src/display/MjpegFrame.hpp"
#include <vector>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
struct Reader { std::vector<uint8_t> data;size_t at=0;int read(){return at<data.size()?data[at++]:-1;} };
int main(int argc,char** argv){
 // Embedded EOI in APP metadata, stuffed FF and restart markers in image data.
 std::vector<uint8_t> frame={0xff,0xd8,0xff,0xe1,0,6,0xff,0xd9,1,2,0xff,0xda,0,2,1,0xff,0,2,0xff,0xd0,3,0xff,0xd9};
 uint8_t output[128];size_t n=0;Reader r{frame};r.data.insert(r.data.end(),frame.begin(),frame.end());
 assert(media::readMjpegFrame(r,output,sizeof(output),n)==media::FrameResult::Frame && n==frame.size());
 assert(media::readMjpegFrame(r,output,sizeof(output),n)==media::FrameResult::Frame);
 assert(media::readMjpegFrame(r,output,sizeof(output),n)==media::FrameResult::End);
 Reader truncated{frame};truncated.data.pop_back();assert(media::readMjpegFrame(truncated,output,sizeof(output),n)==media::FrameResult::Invalid);
 Reader small{frame};assert(media::readMjpegFrame(small,output,8,n)==media::FrameResult::TooLarge);
 Reader progressive{{0xff,0xd8,0xff,0xc2}};assert(media::readMjpegFrame(progressive,output,sizeof(output),n)==media::FrameResult::Unsupported);
 Reader prefix{{'h','e','a','d','e','r'}};prefix.data.insert(prefix.data.end(),frame.begin(),frame.end());assert(media::readMjpegFrame(prefix,output,sizeof(output),n)==media::FrameResult::Frame);
 Reader junk{std::vector<uint8_t>(100000,0)};assert(media::readMjpegFrame(junk,output,sizeof(output),n)==media::FrameResult::Invalid);assert(junk.at<=65536);
 puts("PASS: MJPEG consecutive frames, metadata, truncation, capacity, progressive rejection and bounded scanning");
 if(argc>1){std::ifstream file(argv[1],std::ios::binary);assert(file);Reader actual{{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()}};std::vector<uint8_t> buffer(512*1024);size_t count=0;
  for(;;){auto result=media::readMjpegFrame(actual,buffer.data(),buffer.size(),n);if(result==media::FrameResult::End)break;assert(result==media::FrameResult::Frame);++count;}
  assert(count>0);std::printf("PASS: parsed %zu frames from real MJPEG file\n",count);}
}
