fn main() {
    let i = 1;
    while i <= 20 {
        if i % 15 == 0 {
            print(1515);
        } else {
            if i % 3 == 0 {
                print(333);
            } else {
                if i % 5 == 0 {
                    print(555);
                } else {
                    print(i);
                }
            }
        }
        i = i + 1;
    }
    return 0;
}
