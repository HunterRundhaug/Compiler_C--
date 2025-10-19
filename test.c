int x, y;
int f(int x, int y, int z) {
}

int g() {
  int x, y;
  if (x == y) {
    g();
    x = 12;
    f(y, x, 93);
  }
  else
    while (x > y)
      ;
  return 5;
}