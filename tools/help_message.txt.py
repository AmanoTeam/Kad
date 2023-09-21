import argparse

parser = argparse.ArgumentParser(
    prog = "kad",
    description = None,
    allow_abbrev = False,
    epilog = "Note, options that take an argument require a equal sign. E.g. --address=ADDRESS"
)

parser.add_argument(
    "--host",
    required = False,
    help = "Bind socket to this host. [default: 127.0.0.1]"
)

parser.add_argument(
    "--port",
    required = False,
    help = "Bind socket to this port. [default: 4000]"
)

parser.add_argument(
    "--target",
    required = False,
    help = "Impersonate this target. [default: chrome116]"
)

parser.add_argument(
    "--version",
    action = "store_true",
    help = "Display the Kad version and exit."
)

print("Saving to ./help_message.txt")

with open(file = "./help_message.txt", mode = "w") as file:
    parser.print_help(file = file)