//
//  main.cpp
//  async
//
//  Created by Jason on 2023/2/3.
//

#include <iostream>
#include <thread>
#include <future>
#include <unistd.h>

int decode_event(int a) {
    std::thread::id tid = std::this_thread::get_id();

    for (int i = 0; i < 100000; i++) {
        sleep(1);
        printf("%d %d\n", a * i, tid);
    }
    return a * 2;
}
int main(int argc, const char * argv[]) {
    // insert code here...
    std::cout << "Hello, World!\n";
    
    auto getNum = std::async(std::launch::async, decode_event, 2);
    
//    auto getRet = getNum.get();
//    std::cout << getRet << std::endl; // 4
    std::thread::id tid = std::this_thread::get_id();
    printf("%d %d\n", 1, tid);

    return 0;
}
