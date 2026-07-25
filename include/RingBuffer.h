#include <stdint.h>
template<typename T, uint8_t Capacity>
class RingBuffer{
public:
  enum class OverwritePolicy{Reject, Overwrite};
  explicit RingBuffer(OverwritePolicy policy = OverwritePolicy::Reject):
    head_(0), tail_(0), counter_(0), overFlowCount_(0), policy_(policy) {}

    bool push(T& value){
      if (isFull()){
        if (policy_ == OverwritePolicy::Reject) {
          overFlowCount_++;
          return false;}
        head_ = (head_ + 1) % Capacity;
        counter_--;
      }
      buffer_[tail_] = value;
      tail_ = (tail_ + 1) % Capacity;
      counter_++;
      return true;
    }

    bool pop(T& out){
      if (isEmpty()) return false;
      out = buffer_[head_];
      head_ = (head_ + 1) % Capacity;
      counter_--;
      return true;
    }

    bool isFull() const {return counter_ == Capacity;}
    bool isEmpty() const {return counter_ == 0;}
    uint8_t size() const {return counter_;}
    uint8_t capacity() const {return Capacity;}
    uint8_t OverflowCount() const {return overFlowCount_;}
    void emptyBuffer() { head_ = 0; tail_ = 0; counter_ = 0;}
    void resetOverflowCount() { overFlowCount_ = 0;}

private:
  T buffer_[Capacity];
  volatile uint8_t head_;
  volatile uint8_t tail_;
  volatile uint8_t counter_;
  volatile uint8_t overFlowCount_;
  OverwritePolicy policy_;
};
