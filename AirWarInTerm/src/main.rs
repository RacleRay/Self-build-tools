use std::error::Error;
use std::sync::mpsc;
use std::time::{Duration, Instant};
use std::{io, thread};

use crossterm::cursor::{Hide, Show};
use crossterm::event::{Event, KeyCode};
use crossterm::terminal::{EnterAlternateScreen, LeaveAlternateScreen};
use crossterm::{event, terminal, ExecutableCommand};

use AirWardInTerm::{render, frame};
use AirWardInTerm::invaders::Invaders;
use AirWardInTerm::player::Player;


fn main() -> Result <(), Box<dyn Error>> {

    println!("Hello, world!");
    
    // Terminal
    let mut stdout = io::stdout();
    terminal::enable_raw_mode()?;
    stdout.execute(EnterAlternateScreen)?; //into sub screen
    stdout.execute(Hide)?;
    

    // render to the sub terminal in a new thread and using channel to 
    // put frames waiting for rendering.
    let (render_sender, render_receiver) = mpsc::channel();
    let render_thread = thread::spawn(
        move | | {
            let mut out = io::stdout();
            let mut last_frame = frame::new_frame();
            // render
            // init background
            render::render(&mut out, &last_frame, &last_frame,  true);
            loop {
                let cur_frame = match render_receiver.recv() {
                    Ok(x) => x,
                    Err(_) => break,
                };
                // render on the previous frame. unforce to change.
                render::render(&mut out, &last_frame, &cur_frame, false);
                last_frame = cur_frame;
            }
        }
    );


    
    let mut player = Player::new();
    let mut instant = Instant::now();
    let mut invaders = Invaders::new();

    'gameloop: loop {
        let delta = instant.elapsed();
        instant = Instant::now();
        let mut cur_frame = frame::new_frame();

        // keyboard
        while event::poll(Duration::default())? {
            // ? operator: 简化Ok和Err判断
            if let Event::Key(key_event) = event::read()? {
                match key_event.code {
                    KeyCode::Left => player.move_left(),
                    KeyCode::Right => player.move_right(),
                    KeyCode::Char(' ') => {
                        player.shoot();
                    }
                    KeyCode::Esc | KeyCode::Char('q') => {
                        break 'gameloop;
                    }
                    _ => {}
                }
            }
        }

        player.update(delta);
        invaders.update(delta);
        player.detect_hits(&mut invaders);

        let draws: Vec<&dyn frame::Drawable> = vec![&player, &invaders];
        for dra in draws {
            dra.draw(&mut cur_frame);
        }
        let _ = render_sender.send(cur_frame);
        thread::sleep(Duration::from_millis(1)); // wait for frame

        // check
        if invaders.all_killed() {
            break 'gameloop;
        }
        if invaders.reached_bottom() {
            break 'gameloop;
        }
    };

    // resource recycle
    drop(render_sender);
    render_thread.join().unwrap();
    stdout.execute(Show)?;
    stdout.execute(LeaveAlternateScreen)?;
    terminal::disable_raw_mode()?;

    Ok(())
}
