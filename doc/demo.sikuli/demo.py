def scroll():
    t = 0
    while (t < 4 and exists("1329923989289.png")):
        click("1329923989289.png")
        t += 1
        if exists("1329926306734.png"):
            break
    
def show_index():
    click("1329918467092.png")
    click("NewTabBT.png")
    type("file://localhost/Users/cding/258/repos/cbop/doc/prog_man/index.html\n")

def show_link(link):
    click(link)
    scroll()
    click("1329923450175.png")

show_index( )
show_link("MIi6IliTETW.png")
show_link("HIiElIlTH6Il.png")
show_link("TheProqrammi.png")
show_link("EnvironmentV.png")

