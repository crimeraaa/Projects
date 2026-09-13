// Bring in the standard input/output library into scope. By default, we have a
// set of items defined in the standard library brought into the scope of every
// program.
use std::io;
use std::cmp::Ordering;
use rand::Rng;

fn main() {
    println!("Guess the number!");

    let secret_number = rand::thread_rng().gen_range(1..=100);
    loop {
        println!("Please input your guess.");

        // Create a new, mutable `String` instance that we can modify.
        // It is currently empty at this point.
        let mut guess = String::new();

        io::stdin()
            // Append the user's input from stdinand into the string without
            // overwriting the contents.
            .read_line(&mut guess)

            // The above returns a `Result` type. If the variant is `Ok`, we can
            // continue. Otherwise, if the variant is `Err`, then we crash.
            .expect("Failed to read line");

        // The previous `guess` is a mutable string buffer, so parse the number
        // stored within.
        let guess: u32 = match guess.trim().parse() {
            Ok(num) => num,
            Err(_)  => continue,
        };

        println!("You guessed: {guess}");

        match guess.cmp(&secret_number) {
            Ordering::Less    => println!("Too small!"),
            Ordering::Greater => println!("Too big!"),
            Ordering::Equal   => {
                println!("You win!");
                break;
            }
        }
    }
}
