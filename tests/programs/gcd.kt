fn gcd(a, b) {
    while b != 0 {
        let t = b;
        b = a % b;
        a = t;
    }
    return a;
}
fn main() {
    print(gcd(1071, 462));
    print(gcd(17, 5));
    print(gcd(100, 10));
}
