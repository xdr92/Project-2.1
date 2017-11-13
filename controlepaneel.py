import matplotlib
import serial
import tkinter as Tk
import matplotlib.animation as animation
matplotlib.use('TkAgg')
import math
from serial import SerialException
import struct

from numpy import arange, sin, pi
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from tkinter import messagebox

class GUI:
    def make_gui(self):
        self.connection()
        self.connectionSettings()
        self.setting()
        self.status()
        self.graph()

    def setting(self):
        self.controlframe = Tk.LabelFrame(text="Control", width=500, height=240)
        self.controlframe.grid(row=0, column=0, sticky=Tk.W)
        self.controlframe.grid_propagate(0)

        self.temperature_slider_label = Tk.Label(self.controlframe, text="Temperature threshold (in degree celsius):")
        self.temperature_slider = Tk.Scale(self.controlframe, from_=0, to=60, orient=Tk.HORIZONTAL, length=490)
        self.light_slider_label = Tk.Label(self.controlframe, text="Light intensity threehold (in percentage):")
        self.light_slider = Tk.Scale(self.controlframe, from_=0, to=100, orient=Tk.HORIZONTAL, length=490)

        self.open_button = Tk.Button(self.controlframe, text="Open", command=self.open_screen, width=22, height=2)
        self.auto_button = Tk.Button(self.controlframe, text="Automatisch", command=self.auto_screen, width=22, height=2)
        self.close_button = Tk.Button(self.controlframe, text="Close", command=self.close_screen, width=22, height=2)
        self.set_scale_button = Tk.Button(self.controlframe, text="Set", command=self.set_slider_values, width=22, height=2)
        self.default_button = Tk.Button(self.controlframe, text="Default", command=self.default, width=22, height=2)

        self.temperature_slider_label.grid(row=0, column=0, columnspan=3)
        self.temperature_slider.grid(row=1, column=0, columnspan=3)
        self.light_slider_label.grid(row=2, column=0, columnspan=3)
        self.light_slider.grid(row=3, column=0, columnspan=3)
        self.open_button.grid(row=4, column=0)
        self.auto_button.grid(row=4, column=1)
        self.close_button.grid(row=4, column=2)
        self.set_scale_button.grid(row=5, column=0, columnspan = 2)
        self.default_button.grid(row=5, column=1, columnspan = 4)

    def status(self):
        padx = 10  # Horizontale afstand tot de window
        #settings
        self.temperature_text = 45
        self.scale_light_intensity_text = 43
        self.rolling_shutter_text = "Open"
        self.scale_rolling_shutter_distance_text = 1.6
        self.scale_percent_open_text = 20
        self.scale_percent_closed_text = 80
        self.scale_avg_temperature_text = 23
        self.scale_avg_light_intensity_text = 34

        self.temperature = Tk.IntVar()
        self.scale_light_intensity = Tk.IntVar()
        self.rolling_shutter = Tk.Variable()
        self.scale_rolling_shutter_distance = Tk.IntVar()
        self.scale_percent_open = Tk.IntVar()
        self.scale_percent_closed = Tk.IntVar()
        self.scale_avg_temperature = Tk.IntVar()
        self.scale_avg_light_intensity = Tk.IntVar()

        self.default()

        statusframe = Tk.LabelFrame(text="Status: ", width=350, height=240)
        statusframe.grid(row=0, column=1, sticky=Tk.W)
        statusframe.grid_propagate(0)

        temperature_label = Tk.Label(statusframe, text="Temperature: ")
        light_intensity_label = Tk.Label(statusframe, text="Light intensity: ")
        rolling_shutter_status_label = Tk.Label(statusframe, text="Rolling shutter status: ")
        rolling_shutter_distance_label = Tk.Label(statusframe, text="Rolling shutter distance: ")
        percent_open_label = Tk.Label(statusframe, text="% open: ")
        percent_closed_label = Tk.Label(statusframe, text="% closed: ")
        avg_temperature_label = Tk.Label(statusframe, text="Avg. temperature: ")
        avg_light_intensity_label = Tk.Label(statusframe, text="Avg. light intensity: ")

        self.temperature_status_label = Tk.Label(statusframe, textvariable=self.temperature)
        self.light_intensity_status_label = Tk.Label(statusframe, textvariable=self.scale_light_intensity)
        self.rolling_shutter_status_status_label = Tk.Label(statusframe, textvariable=self.rolling_shutter)
        self.rolling_shutter_distance_status_label = Tk.Label(statusframe, textvariable=self.scale_rolling_shutter_distance)
        self.percent_open_status_label = Tk.Label(statusframe, textvariable=self.scale_percent_open)
        self.percent_closed_status_label = Tk.Label(statusframe, textvariable=self.scale_percent_closed)
        self.avg_temperature_status_label = Tk.Label(statusframe, textvariable=self.scale_avg_temperature)
        self.avg_light_intensity_status_label = Tk.Label(statusframe, textvariable=self.scale_avg_light_intensity)

        temperature_label.grid(row=0, column=0, sticky=Tk.W, padx = padx)
        light_intensity_label.grid(row=1, column=0, sticky=Tk.W, padx = padx)
        rolling_shutter_status_label.grid(row=2, column=0, sticky=Tk.W, padx = padx)
        rolling_shutter_distance_label.grid(row=3, column=0, sticky=Tk.W, padx = padx)
        percent_open_label.grid(row=4, column=0, sticky=Tk.W, padx = padx)
        percent_closed_label.grid(row=5, column=0, sticky=Tk.W, padx = padx)
        avg_temperature_label.grid(row=6, column=0, sticky=Tk.W, padx = padx)
        avg_light_intensity_label.grid(row=7, column=0, sticky=Tk.W, padx = padx)

        self.temperature_status_label.grid(row=0, column=1)
        self.light_intensity_status_label.grid(row=1, column=1)
        self.rolling_shutter_status_status_label.grid(row=2, column=1)
        self.rolling_shutter_distance_status_label.grid(row=3, column=1)
        self.percent_open_status_label.grid(row=4, column=1)
        self.percent_closed_status_label.grid(row=5, column=1)
        self.avg_temperature_status_label.grid(row=6, column=1)
        self.avg_light_intensity_status_label.grid(row=7, column=1)

    def open_screen(self):
        self.sending_items[3] = 1

    def auto_screen(self):
        self.sending_items[3] = 0

    def close_screen(self):
        self.sending_items[3] = 2

    def set_slider_values(self):
        self.sending_items[1] = self.temperature_slider.get()
        self.sending_items[2] = self.light_slider.get()

    def default(self):
        self.temperature_slider.set(15)
        self.light_slider.set(35)

    def write_output_data(self):
        for i in self.sending_items:
            self.ser.write(struct.pack('>B', i))

    def connection(self):
        self.portName = ""
        while self.portName == "":
            for i in range(1, 100):
                portName = 'COM' + str(i)
                try:
                    self.ser = serial.Serial(portName, 19200)
                    self.portName = portName
                    self.right_value_set = False
                    break
                except SerialException:
                    if i != 99:
                        pass
                    else:
                        messagebox.showinfo("Connection error", "Please check the connection between the pc and the arduino and press OK...")
                        break


    def connectionSettings(self):
        #self.ser = serial.Serial(self.portName, 19200)  # open serial port
        self.received_data = []
        self.temperature_data = []
        self.condition_data = []
        self.xas_condition_data = []
        self.avg_temperature_date = []
        self.avg_light_data = []
        self.light_data = []
        self.xas_temperature_data = []
        self.xas_light_data = []
        self.input_counter = 0
        self.get_input = True
        self.right_value_set = False
        self.sending_items = [127, 35, 15, 0]

    def calculate_input_data(self, i):
        try:
            self.write_output_data()
        except SerialException:
            self.connection()
        begin_value = 127
        if self.right_value_set == False:
            for i in range(0, 8):
                s = self.ser.read()
                hex_value = s.hex()
                decimal = int(hex_value, 16)
                if decimal == begin_value:
                    self.right_value_set = True
                    break

        for i in range(0, 7):
            try:
                self.s = self.ser.read()
            except SerialException:
                self.connection()
                break
            hex_value = self.s.hex()
            decimal = int(hex_value, 16)
            if decimal == begin_value:
                s = self.ser.read()
                hex_value = s.hex()
                decimal = int(hex_value, 16)
            self.received_data.append(decimal)
        if self.right_value_set == True:
            #self.write_output_data()
            self.set_label_text()
            self.clear_data_after_day()
            self.set_data_in_right_array()
            self.animate_temperature()
            self.animate_light()
            self.animate_condition()
            self.received_data = []

    def set_label_text(self):
        if self.received_data[2] == 0:
            rolling_shutter = "closed"
        elif self.received_data[2] == 1:
            rolling_shutter = "closing"
        elif self.received_data[2] == 2:
            rolling_shutter = "opening"
        elif self.received_data[2] == 3:
            rolling_shutter = "open"
        else:
            self.received_data = "Error"

        self.temperature.set(str(self.received_data[0]) + "°C")
        self.scale_light_intensity.set(str(self.received_data[1]) + "%")
        self.rolling_shutter.set(rolling_shutter)
        self.scale_rolling_shutter_distance.set(str(math.ceil((self.received_data[4]/100*140+20)*100)/100) + "cm")
        self.scale_percent_open.set(str(self.received_data[3]) + "%")
        self.scale_percent_closed.set(str(self.received_data[4]) + "%")
        self.scale_avg_temperature.set(str(self.received_data[5]) + "°C")
        self.scale_avg_light_intensity.set(str(self.received_data[6]) + "%")

    def set_data_in_right_array(self):
        self.temperature_data.append(self.received_data[0])
        self.light_data.append(self.received_data[1])
        self.condition_data.append(self.received_data[2])
        self.xas_condition_data.append(len(self.condition_data))
        self.xas_temperature_data.append(len(self.temperature_data))
        self.xas_light_data.append(len(self.light_data))
        self.avg_temperature_date.append(self.received_data[5])
        self.avg_light_data.append(self.received_data[6])

    def clear_data_after_day(self):
        if len(self.light_data) > 23:
            self.light_data = []
            self.xas_light_data = []
            self.avg_light_data = []
        if len(self.temperature_data) > 23:
            self.temperature_data = []
            self.xas_temperature_data = []
            self.avg_temperature_date = []
        if len(self.condition_data) > 23:
            self.condition_data = []
            self.xas_condition_data = []

    def animate_temperature(self):
        self.at.clear()
        titels = ["Temperature", "Average"]
        self.at.plot(self.xas_temperature_data, self.temperature_data, label=titels[0])
        self.at.plot(self.xas_temperature_data, self.avg_temperature_date, label=titels[1])
        self.at.legend(loc=2,  shadow=True, title="Legend", fancybox=True, bbox_to_anchor=(1.05, 1), borderaxespad=0.)
        self.at.get_legend()
        self.at.set_title('Temperature')
        self.at.set_xlabel('Time in hours')
        self.at.set_ylabel('Degrees in celcius')
        self.at.grid(True)
        self.at.set_ylim(bottom = 0, top =60)

    def animate_light(self):
        self.al.clear()
        titels = ["Light intensity", "Average"]
        self.al.plot(self.xas_light_data, self.light_data, label=titels[0])
        self.al.plot(self.xas_light_data, self.avg_light_data, label=titels[1])
        self.al.legend(loc=2, shadow=True, title="Legend", fancybox=True, bbox_to_anchor=(1.05, 1), borderaxespad=0.)
        self.al.get_legend()
        self.al.set_title('Light intensity')
        self.al.set_xlabel('Time in hours')
        self.al.set_ylabel('Light percentage')
        self.al.grid(True)
        self.al.set_ylim(bottom=0, top=100)


    def animate_condition(self):
        self.ac.clear()
        titels = ["Condition", "Average"]
        self.ac.plot(self.xas_condition_data, self.condition_data, label=titels[0])
        self.ac.legend(loc=2, shadow=True, title="Legend", fancybox=True, bbox_to_anchor=(1.05, 1), borderaxespad=0.)
        self.ac.get_legend()
        self.ac.set_title('Status')
        self.ac.set_xlabel('Time in hours')
        self.ac.yaxis.set_ticklabels(['closed',"closing", 'opening', "open"])
        self.ac.grid(True)
        self.ac.set_ylim(bottom=0, top=3)

    def graph(self):
        f = Figure(figsize=(9, 6), dpi=100)
        self.at = f.add_subplot(311)
        self.al = f.add_subplot(312)
        self.ac = f.add_subplot(313)
        t = arange(0.0, 3.0, 0.01)
        s = sin(2 * pi * t)
        f.subplots_adjust(hspace = .9, wspace = 4, right = 0.7)
        self.at.plot(t, s)

        #makes frame for graph
        graphframe = Tk.LabelFrame(text="Graphs: ", width=850, height=600)
        graphframe.grid(row=1, column=0, columnspan = 2)
        graphframe.grid_propagate(0)

        # a tk.DrawingArea
        canvas = FigureCanvasTkAgg(f, master=graphframe)
        canvas.show()
        canvas.get_tk_widget().grid()

        canvas._tkcanvas.grid()

        calc_temp = animation.FuncAnimation(f, self.calculate_input_data, interval = 1000)
        #send_data = animation.FuncAnimation(f, self.write_output_data, interval = 1000)
        #ani_temperature = animation.FuncAnimation(f, self.animate_temperature, interval = 1)
        #ani_light = animation.FuncAnimation(f, self.animate_light, interval = 1)
        #ani_condition = animation.FuncAnimation(f, self.animate_condition, interval = 1)

        Tk.mainloop()

root = Tk.Tk()
root.title("Control panel")
root.resizable(False, False)
gui = GUI().make_gui()
