use crate::{NUM_ROWS, NUM_COLS};

pub type Frame = Vec<Vec<&'static str>>;

pub fn new_frame() -> Frame {
    let mut rows = Vec::with_capacity(NUM_ROWS);
    for _ in 0..NUM_ROWS {
        let mut row = Vec::with_capacity(NUM_COLS);
        for _ in 0..NUM_COLS {
            row.push(" ");
        }
        rows.push(row);
    }
    return rows
}

// trait for game objects
pub trait Drawable {
    fn draw(&self, frame: &mut Frame);
}
