use crate::frame::{Drawable, Frame};
use crate::invaders::Invaders;
use crate::bullets::Bullet;
use crate::{NUM_ROWS, NUM_COLS};
use std::time::Duration;


pub struct Player {
    x: usize,
    y: usize,
    bullets: Vec<Bullet>,
}

impl Player {
    pub fn new() -> Self {
        Self {
            x: NUM_ROWS / 2,
            y: NUM_COLS - 1,
            bullets: Vec::new(),
        }
    }
    pub fn move_left(&mut self) {
        if self.x > 0 {
            self.x -= 1;
        }
    }
    pub fn move_right(&mut self) {
        if self.x < NUM_ROWS - 1 {
            self.x += 1;
        }
    }
    // when push SPACE key.
    pub fn shoot(&mut self) -> bool {
        if self.bullets.len() < 10 {
            self.bullets.push(Bullet::new(self.x, self.y - 1));
            true
        } else {
            false
        }
    }
    // retain bullets state
    pub fn update(&mut self, delta: Duration) {
        for bul in self.bullets.iter_mut() {
            bul.update(delta);
        }
        self.bullets.retain(|bul| !bul.dead());
    }
    // detect any bullet hits invader.
    pub fn detect_hits(&mut self, invaders: &mut Invaders) -> bool {
        let mut hit_something = false;
        for bullet in self.bullets.iter_mut() {
            if !bullet.exploding {
                if invaders.kill_invader_at(bullet.x, bullet.y) {
                    hit_something = true;
                    bullet.explode();
                }
            }
        }
        hit_something
    }
}

impl Drawable for Player {
    fn draw(&self, frame: &mut Frame) {
        frame[self.x][self.y] = "W";
        for bullet in self.bullets.iter() {
            bullet.draw(frame);
        }
    }
}
