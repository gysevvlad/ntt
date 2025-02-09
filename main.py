import selectors

sel = selectors.EpollSelector()

while True:
    events = sel.select()
    for key, mask in events:
        print("ooops!")
