import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import json
import os, time
import keycodes

CONFIG_FILE = "controller_map.json"

# Default schema if no JSON exists. 
DEFAULT_MAPPING = {
    "A": "KEY_SPACE", "B": "KEY_CONTROL_L", "X": "KEY_R", "Y": "KEY_E",
    "LB": "MOUSE_2", "RB": "MOUSE_5", "LT": "MOUSE_3", "RT": "MOUSE_1",
    "Select": "Unmapped", "Start": "KEY_ESCAPE", "L Thumb": "KEY_SHIFT_L", "R Thumb": "KEY_V",
    "D-UP": "KEY_1", "D-DOWN": "KEY_3", "D-LEFT": "KEY_2", "D-RIGHT": "KEY_4",
    "Home": "KEY_DELETE", "Center Pad": "Unmapped", "B18" : "Unmapped", "B19": "Unmapped", 
    "B20": "Unmapped", "B21": "Unmapped","B22": "Unmapped", "B23": "Unmapped", 
    "B24": "Unmapped", "B25": "Unmapped", "B26": "Unmapped", "B27": "Unmapped", 
    "B28": "Unmapped", "B29": "Unmapped","B30": "Unmapped", "B31": "Unmapped", 
    "L Stick UP": "KEY_W", "L Stick DOWN": "KEY_S", "L Stick LEFT": "KEY_A", "L Stick RIGHT": "KEY_D"
}

class MappingDialog(tk.Toplevel):
    def __init__(self, parent, btn_name, current_val, save_callback):
        super().__init__(parent)
        self.title(f"Map Button: {btn_name}")
        self.geometry("350x200")
        self.resizable(False, False)
        
        # Make dialog modal
        self.transient(parent)
        self.grab_set()

        self.btn_name = btn_name
        self.new_val = current_val
        self.save_callback = save_callback

        # UI Elements
        ttk.Label(self, text=f"Mapping for {btn_name}", font=("Segoe UI", 12, "bold")).pack(pady=10)
        self.lbl_display = ttk.Label(self, text=f"Current: {current_val}", font=("Segoe UI", 10))
        self.lbl_display.pack(pady=5)
        ttk.Label(self, text="Press any Key or Mouse Button...", foreground="gray").pack(pady=5)

        # Action Buttons Frame
        btn_frame = ttk.Frame(self)
        btn_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=15, padx=10)

        self.btn_save = ttk.Button(btn_frame, text="Save", command=self.apply_save)
        self.btn_save.pack(side=tk.LEFT, expand=True, padx=2)
        
        self.btn_unmap = ttk.Button(btn_frame, text="Unmap", command=self.apply_unmap)
        self.btn_unmap.pack(side=tk.LEFT, expand=True, padx=2)
        
        self.btn_cancel = ttk.Button(btn_frame, text="Cancel", command=self.destroy)
        self.btn_cancel.pack(side=tk.LEFT, expand=True, padx=2)

        # Bind all key and mouse events to this window
        self.bind("<Key>", self.record_event)
        self.bind("<Button>", self.record_event)

        # Force focus so key presses register immediately
        self.focus_set()

    def record_event(self, event):
        # Prevent clicks on the UI buttons from being recorded as mappings
        if event.widget in (self.btn_save, self.btn_cancel, self.btn_unmap):
            return

        if event.type == tk.EventType.KeyPress:
            self.new_val = f"KEY_{event.keysym.upper()}"
        elif event.type == tk.EventType.ButtonPress:
            self.new_val = f"MOUSE_{event.num}" 
        else:
            return

        self.lbl_display.config(text=f"New: {self.new_val}")

    def apply_save(self):
        self.save_callback(self.btn_name, self.new_val)
        self.destroy()

    def apply_unmap(self):
        self.save_callback(self.btn_name, "Unmapped")
        self.destroy()


class ControllerApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("UART Controller Configurator")
        self.geometry("620x550") # Slightly wider to accommodate scrollbar

        self.iconbitmap("controller.ico")
        
        self.mapping = self.load_config()
        self.serial_conn = None
        
        self.setup_ui()
        self.refresh_ports()

    def load_config(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, 'r') as f:
                    return json.load(f)
            except json.JSONDecodeError:
                pass
        return DEFAULT_MAPPING.copy()

    def save_config(self):
        with open(CONFIG_FILE, 'w') as f:
            json.dump(self.mapping, f, indent=4)

    def setup_ui(self):
        # --- Top Frame: Serial Connection ---
        serial_frame = ttk.LabelFrame(self, text="Serial Connection")
        serial_frame.pack(fill=tk.X, padx=10, pady=10)

        self.port_var = tk.StringVar()
        self.cb_ports = ttk.Combobox(serial_frame, textvariable=self.port_var, state="readonly", width=15)
        self.cb_ports.pack(side=tk.LEFT, padx=10, pady=10)

        btn_refresh = ttk.Button(serial_frame, text="↻ Refresh", command=self.refresh_ports, width=10)
        btn_refresh.pack(side=tk.LEFT, padx=5)

        self.btn_connect = ttk.Button(serial_frame, text="Connect", command=self.toggle_connection)
        self.btn_connect.pack(side=tk.LEFT, padx=5)

        self.btn_send = ttk.Button(serial_frame, text="Send Config", command=self.send_configuration, state=tk.DISABLED)
        self.btn_send.pack(side=tk.RIGHT, padx=10)

        # --- Status Bar ---
        self.status_var = tk.StringVar(value="Status: Disconnected")
        status_label = ttk.Label(self, textvariable=self.status_var, foreground="blue")
        status_label.pack(fill=tk.X, padx=10)

        # --- Middle Frame: Scrollable Controller Grid ---
        mapping_container = ttk.LabelFrame(self, text="Controller Mapping")
        mapping_container.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # Canvas and Scrollbar setup
        self.canvas = tk.Canvas(mapping_container, highlightthickness=0)
        self.scrollbar = ttk.Scrollbar(mapping_container, orient="vertical", command=self.canvas.yview)
        
        # Inner frame attached to the canvas
        self.grid_frame = ttk.Frame(self.canvas)

        # Update scrollregion when the inner frame resizes
        self.grid_frame.bind(
            "<Configure>",
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        )

        # Create window inside canvas and update its width when canvas resizes
        self.canvas_window = self.canvas.create_window((0, 0), window=self.grid_frame, anchor="nw")
        self.canvas.bind('<Configure>', lambda e: self.canvas.itemconfig(self.canvas_window, width=e.width))

        self.canvas.configure(yscrollcommand=self.scrollbar.set)
        
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Bind Mousewheel scrolling
        self.canvas.bind_all("<MouseWheel>", self.on_mousewheel)

        self.button_widgets = {}
        self.render_button_grid()

    def on_mousewheel(self, event):
        # Windows-based delta translation for smooth scrolling
        self.canvas.yview_scroll(int(-1*(event.delta/120)), "units")

    def render_button_grid(self):
        for widget in self.grid_frame.winfo_children():
            widget.destroy()

        # Dynamic Grid Layout Variables
        columns = 4
        row, col = 0, 0

        for btn_name, mapped_val in self.mapping.items():
            cell = ttk.Frame(self.grid_frame, borderwidth=1, relief="solid")
            cell.grid(row=row, column=col, padx=5, pady=5, sticky="nsew")
            
            ttk.Label(cell, text=btn_name, font=("Segoe UI", 9, "bold")).pack(pady=(5, 0))
            
            lbl_val = ttk.Label(cell, text=mapped_val, foreground="gray")
            lbl_val.pack()
            self.button_widgets[btn_name] = lbl_val
            
            ttk.Button(cell, text="Edit", command=lambda b=btn_name: self.open_mapper(b)).pack(pady=(2, 5))

            col += 1
            if col >= columns:
                col = 0
                row += 1

        # Force columns in the inner frame to expand evenly
        for i in range(columns):
            self.grid_frame.columnconfigure(i, weight=1)

    def open_mapper(self, btn_name):
        MappingDialog(self, btn_name, self.mapping[btn_name], self.update_mapping)

    def update_mapping(self, btn_name, new_val):
        self.mapping[btn_name] = new_val
        self.button_widgets[btn_name].config(text=new_val)
        self.save_config()
        self.status_var.set(f"Status: Saved {btn_name} -> {new_val}")

    # --- Serial / UART Logic ---
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.cb_ports['values'] = ports
        if ports:
            self.cb_ports.current(0)
        else:
            self.cb_ports.set("No Ports Found")

    def toggle_connection(self, check = 0):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.serial_conn = None
            self.btn_connect.config(text="Connect")
            self.btn_send.config(state=tk.DISABLED)
            self.status_var.set("Status: Disconnected")
            self.cb_ports.config(state="readonly")
        else:
            port = self.port_var.get()
            if not port or port == "No Ports Found":
                messagebox.showerror("Error", "Please select a valid COM port.")
                return
            try:
                self.serial_conn = serial.Serial(port, baudrate=115200, timeout=1)
                self.btn_connect.config(text="Disconnect")
                self.btn_send.config(state=tk.NORMAL)
                self.cb_ports.config(state=tk.DISABLED)
                self.status_var.set(f"Status: Connected to {port}")
                if check:
                    return True
            except Exception as e:
                if check:
                    pass
                else:
                    messagebox.showerror("Connection Error", f"Failed to connect to {port}\n{e}")

    def send_configuration(self):
        if not self.serial_conn or not self.serial_conn.is_open:
            return
        
        try:
            payload = self.byte_conversion() 
            payload = b''.join(payload)
            print(f"Sending payload: {payload}")

            self.serial_conn.reset_input_buffer()
            self.serial_conn.write(payload)
            self.serial_conn.flush()

            self.toggle_connection()  # Disconnect after sending
            time.sleep(0.5)  # Allow time for the device to process the data
            port_name = self.port_var.get()
            for _ in range(1000):
                ports = [tuple(p) for p in serial.tools.list_ports.comports()]
                connected = any(port_name in port for port in ports)
                if connected:
                    if self.toggle_connection(1):
                        break

            self.status_var.set(f"Status: Sent {len(payload)} bytes to device.")
            
        except Exception as e:
            self.status_var.set("Status: Failed to send data.")
            messagebox.showerror("Transmission Error", str(e))

    def byte_conversion(self):
        final_bytes = [b'\xff', b'\xaa']

        for i in self.mapping:
            if self.mapping[i] in keycodes.asciiToKeycode:
                final_bytes.append(keycodes.asciiToKeycode[self.mapping[i]])
                final_bytes.append(b'\x01')

            elif self.mapping[i] in keycodes.modifier:
                final_bytes.append(keycodes.modifier[self.mapping[i]])
                final_bytes.append(b'\x02')
                
            elif self.mapping[i] in keycodes.mouse:
                final_bytes.append(keycodes.mouse[self.mapping[i]])
                final_bytes.append(b'\x03')

            else:
                final_bytes += [b'\x00', b'\x00']

        return final_bytes

if __name__ == "__main__":
    app = ControllerApp()
    app.mainloop()