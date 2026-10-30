fn main() {
    let x = 1;
    if 1 {
        let x = 2;
        print(x);
        if 1 {
            let x = 3;
            print(x);
        }
        print(x);
    }
    print(x);
}
