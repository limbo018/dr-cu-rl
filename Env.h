#pragma once
class Env {
public:
    void step(float action);
    void rest();
    float state{0};
    float reward{0};
    bool done{false};
private:
  int _count{0};
};
