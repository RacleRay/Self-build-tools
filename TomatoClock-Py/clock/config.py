from pkg_resources import resource_filename


DELAYS = {
    'work': 25 * 60,
    'long_work':  15 * 60,
    'break':  5 * 60,
    'long_break':  15 * 60,
    'not_break': 5 * 60,
    'not_work': 5 * 60,
}

ICON = resource_filename('clock',  'ELDO.png')