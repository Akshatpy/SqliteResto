#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
sqlite3 *db;
GtkWidget *name_entry, *phone_entry;
int current_customer_id = 0;
int current_customer_points = 0; // loaded from DB
typedef struct { int item_id; GtkWidget *spin; float price; } MenuItem;
MenuItem *menu_items = NULL;
int menu_item_count = 0;
void initialize_db();
void load_customer_points();
void show_admin();
void verify_db();
void print_menu();
void on_submit_customer(GtkWidget *widget, gpointer data);
void on_place_order(GtkWidget *widget, gpointer data);
void create_invoice(float total, int used_points, int new_points);
void initialize_db() {
    sqlite3_open("restaurant.db", &db);
    sqlite3_exec(db, "DROP TABLE IF EXISTS menu;", NULL, NULL, NULL);
    const char *schemas[] = {
        "CREATE TABLE IF NOT EXISTS customers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, phone TEXT UNIQUE, points INTEGER DEFAULT 0);",
        "CREATE TABLE IF NOT EXISTS menu (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, price REAL NOT NULL);",
        "CREATE TABLE IF NOT EXISTS orders (id INTEGER PRIMARY KEY AUTOINCREMENT, customer_id INTEGER, items TEXT, total REAL, used_points INTEGER, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);",
        "CREATE TABLE IF NOT EXISTS invoices (id INTEGER PRIMARY KEY AUTOINCREMENT, order_id INTEGER, details TEXT);"
    };
    for (int i = 0; i < 4; i++) sqlite3_exec(db, schemas[i], NULL, NULL, NULL);
    const char *items[] = {"Burger","Pizza","Vada Pav","Salad","Lime Juice","Pasta","Biryani","KitKat","Soup","Chai"};
    const float prices[] = {120,340,50,100,60,90,200,30,110,15};
    sqlite3_stmt *stmt;
    for (int i = 0; i < 10; i++) {
        sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO menu (name, price) VALUES (?, ?);", -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, items[i], -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, prices[i]);
        sqlite3_step(stmt); sqlite3_finalize(stmt);
    }
    verify_db(); print_menu();
}
void load_customer_points() {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db, "SELECT points FROM customers WHERE id=?;", -1, &s, NULL);
    sqlite3_bind_int(s,1,current_customer_id);
    if (sqlite3_step(s)==SQLITE_ROW) current_customer_points = sqlite3_column_int(s,0);
    sqlite3_finalize(s);
}
void show_admin() {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db, "SELECT name,phone,points FROM customers;", -1, &s, NULL);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Admin: Customers");
    gtk_window_set_default_size(GTK_WINDOW(win), 300, 400);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), vb);
    while (sqlite3_step(s)==SQLITE_ROW) {
        const char *n = (const char*)sqlite3_column_text(s,0);
        const char *p = (const char*)sqlite3_column_text(s,1);
        int pts = sqlite3_column_int(s,2);
        char buf[128];
        sprintf(buf, "%s (%s) - Points: %d", n,p,pts);
        gtk_box_pack_start(GTK_BOX(vb), gtk_label_new(buf), FALSE, FALSE, 2);
    }
    sqlite3_finalize(s);
    gtk_widget_show_all(win);
}
void verify_db() {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM menu;", -1, &s, NULL);
    if (sqlite3_step(s)==SQLITE_ROW) {
        int c=sqlite3_column_int(s,0);
        if (c==0) fprintf(stderr,"DB seed failed.\n");
    }
    sqlite3_finalize(s);
}
void print_menu() {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db, "SELECT id,name,price FROM menu;", -1, &s, NULL);
    while (sqlite3_step(s)==SQLITE_ROW) sqlite3_column_int(s,0);
    sqlite3_finalize(s);
}
void on_submit_customer(GtkWidget *w, gpointer d) {
    const char *nm = gtk_entry_get_text(GTK_ENTRY(name_entry));
    const char *ph = gtk_entry_get_text(GTK_ENTRY(phone_entry));
    if (!*nm||!*ph) return;
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO customers(name,phone) VALUES(?,?);", -1, &s, NULL);
    sqlite3_bind_text(s,1,nm,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,ph,-1,SQLITE_STATIC);
    sqlite3_step(s); sqlite3_finalize(s);
    sqlite3_prepare_v2(db, "SELECT id FROM customers WHERE phone=?;", -1, &s, NULL);
    sqlite3_bind_text(s,1,ph,-1,SQLITE_STATIC);
    if(sqlite3_step(s)==SQLITE_ROW) current_customer_id=sqlite3_column_int(s,0);
    sqlite3_finalize(s);
    load_customer_points();
    GtkWidget *mw=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mw),"Menu");
    gtk_window_set_default_size(GTK_WINDOW(mw),500,450);
    GtkWidget *vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_container_add(GTK_CONTAINER(mw),vb);
    char ptsbuf[64]; sprintf(ptsbuf,"Available Points: %d",current_customer_points);
    gtk_box_pack_start(GTK_BOX(vb),gtk_label_new(ptsbuf),FALSE,FALSE,5);
    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid),10);
    gtk_grid_set_row_spacing(GTK_GRID(grid),5);
    gtk_box_pack_start(GTK_BOX(vb),grid,FALSE,FALSE,5);
    gtk_grid_attach(GTK_GRID(grid),gtk_label_new("Item"),0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid),gtk_label_new("Price"),1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid),gtk_label_new("Qty"),2,0,1,1);
    sqlite3_prepare_v2(db, "SELECT id,name,price FROM menu;", -1, &s, NULL);
    int r=1;
    while(sqlite3_step(s)==SQLITE_ROW){
        int mid=sqlite3_column_int(s,0);
        const char *nm2=(const char*)sqlite3_column_text(s,1);
        float pr=sqlite3_column_double(s,2);
        GtkWidget*l=gtk_label_new(nm2);
        GtkWidget*pl=gtk_label_new(g_strdup_printf("₹%.2f",pr));
        GtkWidget*sp=gtk_spin_button_new_with_range(0,20,1);
        menu_items=realloc(menu_items,(menu_item_count+1)*sizeof(MenuItem));
        menu_items[menu_item_count].item_id=mid;
        menu_items[menu_item_count].spin=sp;
        menu_items[menu_item_count].price=pr;
        menu_item_count++;
        gtk_grid_attach(GTK_GRID(grid),l,0,r,1,1);
        gtk_grid_attach(GTK_GRID(grid),pl,1,r,1,1);
        gtk_grid_attach(GTK_GRID(grid),sp,2,r,1,1);
        r++; }
    sqlite3_finalize(s);
    GtkWidget *use_label=gtk_label_new("Use Points:");
    GtkWidget *use_spin=gtk_spin_button_new_with_range(0,current_customer_points,1);
    gtk_box_pack_start(GTK_BOX(vb),use_label,FALSE,FALSE,2);
    gtk_box_pack_start(GTK_BOX(vb),use_spin,FALSE,FALSE,2);
    GtkWidget*btn=gtk_button_new_with_label("Place Order");
    g_signal_connect(btn,"clicked",G_CALLBACK(on_place_order),use_spin);
    gtk_box_pack_start(GTK_BOX(vb),btn,FALSE,FALSE,5);
    gtk_widget_show_all(mw);
}
void on_place_order(GtkWidget *btn, gpointer data) {
    GtkWidget *use_spin = GTK_WIDGET(data);
    int used = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(use_spin));
    char it[1024]=""; float tot=0;
    for(int i=0;i<menu_item_count;i++){
        int q=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(menu_items[i].spin));
        if(q){ tot += menu_items[i].price*q;
            char b[32]; sprintf(b,"%d:%d,",menu_items[i].item_id,q);
            strcat(it,b);} }
    if(!tot) return;
    if(used>current_customer_points) used=current_customer_points;
    float final_total = tot - used;
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db,"INSERT INTO orders(customer_id,items,total,used_points) VALUES(?,?,?,?);",-1,&s,NULL);
    sqlite3_bind_int(s,1,current_customer_id);
    sqlite3_bind_text(s,2,it,-1,SQLITE_STATIC);
    sqlite3_bind_double(s,3,final_total);
    sqlite3_bind_int(s,4,used);
    sqlite3_step(s); sqlite3_finalize(s);
    int earned = (int)(final_total/10);
    int remaining = current_customer_points - used + earned;
    sqlite3_prepare_v2(db,"UPDATE customers SET points=? WHERE id=?;",-1,&s,NULL);
    sqlite3_bind_int(s,1,remaining);
    sqlite3_bind_int(s,2,current_customer_id);
    sqlite3_step(s); sqlite3_finalize(s);
    create_invoice(final_total,used,earned);
}
void create_invoice(float total,int used_points,int new_points){
    char msg[256];
    sprintf(msg,"Order Summary:\nTotal after discount: ₹%.2f\nPoints used: %d\nPoints earned: %d",total,used_points,new_points);
    GtkWidget*d=gtk_message_dialog_new(NULL,GTK_DIALOG_MODAL,GTK_MESSAGE_INFO,GTK_BUTTONS_OK,msg);
    gtk_dialog_run(GTK_DIALOG(d));gtk_widget_destroy(d);
}
int main(int argc,char**argv){gtk_init(&argc,&argv);initialize_db();
    GtkWidget*w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w),"Restaurant Management Jackfruit Project");gtk_window_set_default_size(GTK_WINDOW(w),350,200);
    GtkWidget*g=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(g),8);gtk_grid_set_column_spacing(GTK_GRID(g),5);
    gtk_container_add(GTK_CONTAINER(w),g);
    gtk_grid_attach(GTK_GRID(g),gtk_label_new("Name:"),0,0,1,1);
    name_entry=gtk_entry_new();gtk_grid_attach(GTK_GRID(g),name_entry,1,0,1,1);
    gtk_grid_attach(GTK_GRID(g),gtk_label_new("Phone:"),0,1,1,1);
    phone_entry=gtk_entry_new();gtk_grid_attach(GTK_GRID(g),phone_entry,1,1,1,1);
    GtkWidget*sbtn=gtk_button_new_with_label("Submit");g_signal_connect(sbtn,"clicked",G_CALLBACK(on_submit_customer),NULL);
    gtk_grid_attach(GTK_GRID(g),sbtn,1,2,1,1);
    GtkWidget*abtn=gtk_button_new_with_label("Admin");g_signal_connect(abtn,"clicked",G_CALLBACK(show_admin),NULL);
    gtk_grid_attach(GTK_GRID(g),abtn,1,3,1,1);
    g_signal_connect(w,"destroy",G_CALLBACK(gtk_main_quit),NULL);
    gtk_widget_show_all(w);gtk_main();sqlite3_close(db);return 0; }
