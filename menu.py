#menu for 0.0.5 release for faster coding/progress.In the next release it will be implented and will be frequently better
import appuifw

appuifw.app.title = u'symdroid'

menulist = [u"Load file", u"Settings", u"Exit"]

def menu():
    main = appuifw.selection_list(menulist)
    
    if main == 0:
        print"Not avilible yet" //import filebrowser #will be compiled into .pyc to can be imported
        
    if main == 1:
        print"Not avilible yet"
        
    if main == 2:
        appuifw.app.set_exit()
        //I deleted some lines because of it's bug
