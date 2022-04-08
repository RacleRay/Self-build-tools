use crate::frame::Frame;
use crossterm::cursor::MoveTo;
use crossterm::style::{Color, SetBackgroundColor};
use crossterm::terminal::{Clear, ClearType};
use crossterm::QueueableCommand;
use std::io::{Stdout, Write};


pub fn render(outstream: &mut Stdout, last_frame: &Frame, cur_frame: &Frame, force: bool) {
    if force {
        outstream.queue(SetBackgroundColor(Color::Grey)).unwrap(); // unwrap: get Ok or panic
        outstream.queue(Clear(ClearType::All)).unwrap();
        outstream.queue(SetBackgroundColor(Color::Black)).unwrap();
    }

    for (rowidx, rowvec) in cur_frame.iter().enumerate() {
        for (colidx, ch) in rowvec.iter().enumerate() {
            if *ch != last_frame[rowidx][colidx] || force {
                outstream.queue(MoveTo(rowidx as u16, colidx as u16)).unwrap();
                print!("{}", *ch);
            }
        }
    }
    
    outstream.flush().unwrap();
}
