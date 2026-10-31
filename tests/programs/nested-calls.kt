fn add(a, b) { return a + b; }
fn one(x) { return x; }
fn main() {
    print(one(add(1, add(2, add(3, 4)))));
    print(add(one(1), one(2)));
    return add(one(1), add(one(2), one(3)));
}
