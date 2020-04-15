from pkg_resources import resource_filename


DELAYS = {
    'work': 25 * 60,
    'long_work':  15 * 60,
    'break':  5 * 60,
    'long_break':  15 * 60,
    'not_break': 5 * 60,
    'not_break_long': 5 * 60,
    'not_work': 5 * 60,
}

# For test
# DELAYS = {
#     'work': 1,
#     'long_work':  1,
#     'break':  1,
#     'long_break':  1,
#     'not_break':1,
#     'not_break_long':1,
#     'not_work':1,
# }

ICON = resource_filename('clock',  'ELDO.png')