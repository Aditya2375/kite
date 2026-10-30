fn collatz_steps(n) {
    let steps = 0;
    while n != 1 {
        if n % 2 == 0 {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        steps = steps + 1;
    }
    return steps;
}

fn main() {
    print(collatz_steps(27));
    let x = 3;
    if x > 5 { print(100); } else { print(200); }
    return 7;
}
