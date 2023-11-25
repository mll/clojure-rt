#include <stdio.h>
#include <stdlib.h>


class Exception {
  public:
  Exception(const char* text)
      : _text(text)
  {
  }

  const char* GetText() const { return _text; }

  private:
  const char* _text;
};

void bar()
{
  throw new Exception("Exception requested by caller");
}

void foo() {
    bar();
}


void coo() {
    printf("aaa");
}

void doo() {
    coo();
}

int main(int argc, const char* argv[])
{
  try {
    doo();
    foo();
  } catch (Exception e) {
    printf("xxx %s", e.GetText());
  }
  return 0;
}