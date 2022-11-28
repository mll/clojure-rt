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

int main(int argc, const char* argv[])
{
  bar();
  return 0;
}