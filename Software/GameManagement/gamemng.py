import tkinter as tk
from tkinter import ttk
import socket
import threading
import json

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    host = "192.168.10.64"
    port = 50000
    print(u"Connecting... host: {} port: {}".format(host, port))
    s.connect((host, port))
    print(u"Connect! host: {} port: {}".format(host, port))
except Exception as e:
    print(e)
    print(u"Cannot connect Server.")
    exit()

# Button Lock Flag
# buttonLock = False


class ServerThread(threading.Thread):
    def __init__(self):
        super(ServerThread, self).__init__()

    def run(self):
        while True:
            try:
                recvmsg = s.recv(1024)
                statusText.set(recvmsg)
                print(recvmsg)
                if (recvmsg.decode('utf-8') == "esp_gamestart"):
                    print("button Lock")
                    # buttonLock = True
                    app.entryBtn.configure(state='disabled')
                else:
                    # buttonLock = False
                    app.entryBtn.configure(state='normal')
            except Exception as e:
                print(e)
                app.entryBtn.configure(state='disabled')
                exit()


class Application(tk.Frame):
    def __init__(self, master):
        super().__init__(master)
        self.master = master
        # self.pack(expand=1, fill=tk.BOTH, anchor=tk.NW)
        self.master.option_add('*font', ('Helvetica', 12))
        self.grid(row=0, column=0, sticky="we")
        self.player = {}
        self.create_widgets()

    def create_widgets(self):
        # name
        self.nameLabel = tk.Label(
            text=u'Player Name')
        self.nameLabel.grid(row=2, column=0, padx=5, pady=5, sticky="E")
        self.nameForm = tk.Entry(width=20)
        self.nameForm.grid(row=2, column=1, padx=5, pady=5, sticky="WE")

        # Team
        self.teamLabel = tk.Label(
            text=u'Team')
        self.teamLabel.grid(row=3, column=0, padx=5, pady=5, sticky="E")
        self.teamForm = ttk.Combobox(
            values=["Red", "Green", "Blue"], state='readonly', width="20")
        self.teamForm.set("Red")
        self.teamForm.grid(row=3, column=1, padx=5, pady=5, sticky="WE")

        # Difficulty
        self.dfcltLabel = tk.Label(
            text=u'Difficulty')
        self.dfcltLabel.grid(row=4, column=0, padx=5, pady=5, sticky="E")
        self.dfcltForm = ttk.Combobox(
            values=["Easy", "Normal", "Hard", "Lunatic"], state='readonly')
        self.dfcltForm.set("Easy")
        self.dfcltForm.grid(row=4, column=1, padx=5, pady=5, sticky="WE")

        # Entry
        self.entryBtn = tk.Button(text=u'Player Entry', command=self.entry)
        # if(buttonLock):
        #     self.entryBtn.configure(state='disabled')
        # else:
        #     self.entryBtn.configure(state='normal')
        self.entryBtn.grid(row=4, column=2, padx=5, pady=5)

        # self.hi_there = tk.Button(self)
        # self.hi_there["text"] = "Hello World\n(click me)"
        # self.hi_there["command"] = self.say_hi
        # self.hi_there.pack(side="top")

        # self.quit = tk.Button(self, text="QUIT", fg="red",
        #                       command=self.master.destroy)
        # self.quit.pack(side="bottom")

        # self.

    def entry(self):
        self.player['startFlag'] = True
        self.player['name'] = self.nameForm.get()

        # Team
        if (self.teamForm.get() == "Red"):
            self.player['team'] = 1
        elif (self.teamForm.get() == "Green"):
            self.player['team'] = 2
        elif (self.teamForm.get() == "Blue"):
            self.player['team'] = 3

        # Difficulty
        if (self.dfcltForm.get() == "Easy"):
            self.player['difficulty'] = 1
        elif (self.dfcltForm.get() == "Normal"):
            self.player['difficulty'] = 2
        elif (self.dfcltForm.get() == "Hard"):
            self.player['difficulty'] = 3
        elif (self.dfcltForm.get() == "Lunatic"):
            self.player['difficulty'] = 4

        s.send(json.dumps(self.player, ensure_ascii = False).encode("UTF-8"))


root = tk.Tk()
root.title(u"Game Management - Trinity Bullet")
# root.geometry("400x300")

# Title
titleFrame = tk.Frame(root)
titleFrame.grid(row=0, rowspan=2,  column=0)
titleFrame.titleLabel = tk.Label(
    text=u'Trinity Bullet', font=('Helvetica', '20', 'bold'), anchor="center")
titleFrame.titleLabel.grid(row=0, column=0, columnspan=3,
                           sticky="wes")
titleFrame.titleLabel = tk.Label(
    text=u'', font=('Helvetica', '12', 'bold'), anchor="center")
titleFrame.titleLabel.grid(row=1, column=0, columnspan=3,
                           sticky="wen", pady=7)
titleFrame.title2Label = tk.Label(
    text=u'Game Management Application', font=('Helvetica', '12'), anchor="center")
titleFrame.title2Label.grid(row=1, column=0, columnspan=3,
                            sticky="wen")

# Data
app = Application(master=root)

# Status
statusText = tk.StringVar()
statusText.set("Status...")
statFrame = tk.Frame(root)
statFrame.grid(row=5, column=0, pady="20")
statFrame.statLabel = tk.Label(
    textvariable=statusText, font=('Helvetica', '12'), relief=tk.RIDGE, bd=1, anchor="w")
statFrame.statLabel.grid(row=5, column=0, columnspan=3,
                         sticky="wes")

th = ServerThread()
th.setDaemon(True)
th.start()

root.mainloop()
