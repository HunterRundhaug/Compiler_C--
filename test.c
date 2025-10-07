int f(int x, int y) {
  return x;
}

int g() {
  f(x, y);  /* x, y are undeclared */
}