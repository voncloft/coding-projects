#pragma once
#include "Arduino.h"
struct Queue {size_t capacity,item;std::deque<std::vector<uint8_t>> q;};
using QueueHandle_t=Queue*;
#define pdTRUE 1
#define configASSERT(x) assert(x)
inline QueueHandle_t xQueueCreate(size_t n,size_t z){return new Queue{n,z,{}};}
inline int xQueueSend(Queue*q,const void*p,int){if(q->q.size()==q->capacity)return 0;q->q.emplace_back((const uint8_t*)p,(const uint8_t*)p+q->item);return 1;}
inline int xQueueReceive(Queue*q,void*p,int){if(q->q.empty())return 0;memcpy(p,q->q.front().data(),q->item);q->q.pop_front();return 1;}
