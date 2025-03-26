#include <stdio.h>
#include <sqlite3.h> //for sqlite database actions
#include <string.h>
#include <stdlib.h>
#include <time.h> //for getting timestamps for the record table
#include <sha256.h> //for hashing password + salt + pepper

#define MAXLEN 1024

/* LIST OF QUERIES/ACTIONS IN THIS DBMS
 * List of cars with a given color (selected via dropdown?)
 * List of cars with a given make (selected via dropdown?)
 * List of cars with a given model (selected via dropdown?)
 * List of cars with a value greater/less than a given value (dropdown to determine which?, value given by input?)
 * List of cars with a given year (given by input?)
 * List of models given a make (selected via dropdown?)
 * Search a car based on the VIN (given by input?)
 * Search a car based on the Lic Plate Digits (selected via dropdown? OR value given by input?)
 * List of cars with a mile per gal greater/less than a given value (dropdown to determine which?, value given by input?)
 * Employee Login (username & password given by input?) (pepper stored in this script, pepper in database)
 * Add a car to the database (can only be done by logged in employees)
 * Remove a car from the database (can only be done by logged in employees)
 * List of cars with a mileage greater/less than a given value (dropdown to determine which?, value given by input?)
 *
 * FOR ADD/REMOVE CAR AND EMPLOYEE LOGIN ABOVE TAKEN, A RECORD WILL BE ADDED TO THE DATABASE WITH WHAT HAPPENED
*/

//prototype for function connect, to get the database connection object
sqlite3* Connect();

//prototype for callback function to get the count of a query
int callbackCount(void *, int, char **,char **);

//prototypes for functions related to getting and parsing form data
void get_post_data(char *, size_t);
void read_post_data(char *, int);
void url_decode(char *);

//prototype for the function that adds records to the database (called for some actions such as login, add/remove cars, etc)
void AddRecord(sqlite3*, char [], int);

int main()
{
    char *method = getenv("REQUEST_METHOD");

    // Handle GET request (when the page is first loaded)
    if (strcmp(method, "GET") == 0)
    {
        char *query_string = getenv("QUERY_STRING");

        //RemoveCar.html GET logic
        if (query_string && strstr(query_string, "page=RemoveCar"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            sqlite3* db = Connect(); // Connect to the database
            sqlite3_stmt *stmt;
            int rc;

            // Query to get car names
            const char *sql = "SELECT * FROM Car;";
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK)
            {
                printf("<p>Error preparing query: %s</p>\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return 1;
            }

            // Generate the form
            printf("<h1 class=\"main_container\">Remove a Car</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"remove\">\n");
            printf(" <input type=\"hidden\" name=\"action\" value=\"remove\">\n");

            printf("  <label for=\"car\">Select Car to Remove:</label>\n");

            // Fetch and display car names as datalist options
            printf("<select style=\"width: 500px;\" id=\"car\" name=\"remove\" required>\n");
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                char query[MAXLEN];
                snprintf(query, sizeof(query),
                         "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                         sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                sqlite3_stmt *stmt2;
                sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                sqlite3_step(stmt2);

                printf("    <option value=\"%d\">%s %s %s Valued at $%.2f VIN:%d Id:%d</option>\n",
                       sqlite3_column_int(stmt, 0),
                       sqlite3_column_text(stmt, 1),
                       sqlite3_column_text(stmt2, 0),
                       sqlite3_column_text(stmt2, 1),
                       sqlite3_column_double(stmt, 5),
                       sqlite3_column_int(stmt, 9),
                       sqlite3_column_int(stmt, 0));
            }

            printf("</select>\n");
            printf("  <input type=\"submit\" value=\"Remove\">\n");
            printf("</form>\n");

            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        //AddCar.html GET logic
        else if (query_string && strstr(query_string, "page=AddCar"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            sqlite3* db = Connect(); // Connect to the database
            sqlite3_stmt *stmt;
            int rc;

            // Query to get car make and models
            const char *sql = "SELECT Make.Id, Model.Id, Make.Name, Model.Name FROM Make, Model WHERE Make.Id = Model.Make;";
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK)
            {
                printf("<p>Error preparing query: %s</p>\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return 1;
            }


            // Generate the form
            printf("<h1 class=\"main_container\">Add a Car</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"AddCar\">\n");
            printf(" <input type=\"hidden\" name=\"action\" value=\"add\">\n");

            //make & model
            printf("  <label for=\"make\">Select a Make and Model:</label>\n");
            printf("<select style=\"width: 500px;\" id=\"make\" name=\"make_model\" required>\n");

            // Fetch and display car names as datalist options
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                // Retrieve make and model IDs and their names
                int make_id = sqlite3_column_int(stmt, 0);
                int model_id = sqlite3_column_int(stmt, 1);
                const char *make_name = (const char *)sqlite3_column_text(stmt, 2);
                const char *model_name = (const char *)sqlite3_column_text(stmt, 3);

                // Set the option value to be a combination of make_id and model_id, separated by a colon
                printf("<option value=\"%d:%d\">%s %s</option>\n", make_id, model_id, make_name, model_name);
            }

            printf("</select><br>\n");

            //year
            printf("  <label for=\"year\">Year:</label>\n");
            printf("  <input type=\"number\" name=\"year\" id=\"year\" min=\"1900\" max=\"2025\" value=\"2025\" required><br>\n");

            //mileage
            printf("  <label for=\"mileage\">Mileage (in miles):</label>\n");
            printf("  <input type=\"number\" name=\"mileage\" id=\"mileage\" step=\"1\" value=\"10000\" required><br>\n");

            //value
            printf("  <label for=\"value\">Value (in dollars):</label>\n");
            printf("  <input type=\"number\" name=\"value\" id=\"value\" step=\"1\" value=\"15000\" required><br>\n");

            //vin
            printf("  <label for=\"vin\">VIN:</label>\n");
            printf("  <input type=\"text\" name=\"vin\" id=\"vin\" minlength=\"17\" maxlength=\"17\" required><br>\n");

            //mi/gal
            printf("  <label for=\"mpg\">Miles per Gallon:</label>\n");
            printf("  <input type=\"number\" name=\"mpg\" id=\"mpg\" step=\"0.1\" value=\"25\" required><br>\n");

            //lic plate digits
            printf("  <label for=\"license_plate\">License Plate:</label>\n");
            printf("  <input type=\"text\" name=\"license_plate\" id=\"license_plate\" minlength=\"6\" maxlength=\"6\" required><br>\n");

            //color
            printf("  <label for=\"color\">Car Color:</label>\n");
            printf("  <input type=\"text\" name=\"color\" id=\"color\" required><br>\n");

            //end of the form
            printf("  <input type=\"submit\" value=\"Add\">\n");
            printf("</form>\n");

            //close all the statements and the database connection
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        //EmployeeLogin.html GET logic
        else if (query_string && strstr(query_string, "page=EmployeeLogin"))
        {

        }
        //FindByLicPlate.html GET logic
        else if (query_string && strstr(query_string, "page=FindByLicPlate"))
        {

        }
        //FindByVIN.html GET logic
        else if (query_string && strstr(query_string, "page=FindByVin"))
        {

        }
        //ListByColor.html GET logic
        else if (query_string && strstr(query_string, "page=ListByColor"))
        {

        }
        //ListByMake.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMake"))
        {

        }
        //ListByMileage.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMileage"))
        {

        }
        //ListByMilePerGal.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMilePerGal"))
        {

        }
        //ListByModel.html GET logic
        else if (query_string && strstr(query_string, "page=ListByModel"))
        {

        }
        //ListByValue.html GET logic
        else if (query_string && strstr(query_string, "page=ListByValue"))
        {

        }
        //ListByYear.html GET logic
        else if (query_string && strstr(query_string, "page=ListByYear"))
        {

        }
        //ListModelByMake.html GET logic
        else if (query_string && strstr(query_string, "page=ListModelByMake"))
        {

        }
        //otherwise, go to the main screen (as a backup or if loaded via this script instead of by html page)
        else
        {
            printf("Content-Type: text/html\n");
            printf("Status: 302 Found\n");
            printf("Location: /home.html\n\n");
        }

    }
    //handle post requests (when a form is submitted)
    else if (strcmp(method, "POST") == 0)
    {
        //print header info
        printf("Content-type: text/html\n\n");

        char post_data[MAXLEN];
        read_post_data(post_data, MAXLEN);
        char post_data_2[MAXLEN];
        strcpy(post_data_2, post_data);

        // Determine the action
        char *action = strstr(post_data, "action=");
        if (action)
        {
            action += 7;
            if (strncmp(action, "remove", 6) == 0)
            {
                char *car = strstr(post_data, "remove=");
                if (car)
                {
                    int car_id = atoi(car + 7);

                    sqlite3* db = Connect();

                    //extract car value from the post data
                    char *car_value = car + 4;

                    //extract the car id value (from the end of the extracted car data)
                    const char delim[] = "=";
                    char *token;
                    char *lastToken = NULL;

                    //tokenize the string
                    token = strtok(car_value, delim);
                    while (token != NULL)
                    {
                        lastToken = token;
                        token = strtok(NULL, delim);
                    }

                    //convert last token (car id) to integer
                    int id = 0;
                    if (lastToken != NULL)
                    {
                        id = atoi(lastToken);
                    }

                    //delete from database (based on the car id)
                    char* errMssg;
                    char query[MAXLEN];
                    snprintf(query, sizeof(query),
                             "DELETE FROM Car WHERE Id = %d", id);
                    sqlite3_exec(db, query, NULL ,0, &errMssg);

                    //add the record to the database (still need the id of the employee from POST)


                    //print the form here again
                    sqlite3_stmt *stmt;
                    int rc;

                    // Query to get car names
                    const char *sql = "SELECT * FROM Car;";
                    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                    if (rc != SQLITE_OK)
                    {
                        printf("<p>Error preparing query: %s</p>\n", sqlite3_errmsg(db));
                        sqlite3_close(db);
                        return 1;
                    }

                    // Generate the form
                    printf("<h1 class=\"main_container\">Remove a Car</h1>\n");
                    printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                    // Hidden input to pass the page context
                    printf("<input type=\"hidden\" name=\"page\" value=\"remove\">\n");
                    printf(" <input type=\"hidden\" name=\"action\" value=\"remove\">\n");

                    printf("  <label for=\"car\">Select Car to Remove:</label>\n");

                    // Fetch and display car names as datalist options
                    printf("<select style=\"width: 500px;\" id=\"car\" name=\"remove\" required>\n");
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        char query[MAXLEN];
                        snprintf(query, sizeof(query),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("    <option value=\"%d\">%s %s %s Valued at $%.2f VIN:%d Id:%d</option>\n",
                               sqlite3_column_int(stmt, 0),
                               sqlite3_column_text(stmt, 1),
                               sqlite3_column_text(stmt2, 0),
                               sqlite3_column_text(stmt2, 1),
                               sqlite3_column_double(stmt, 5),
                               sqlite3_column_int(stmt, 9),
                               sqlite3_column_int(stmt, 0));
                    }

                    printf("</select>\n");
                    printf("  <input type=\"submit\" value=\"remove\">\n");
                    printf("</form>\n");

                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                }
            }
            else if (strncmp(action, "add", 3) == 0)
            {
                char *car_data = strstr(post_data_2, "add");
                int make_id;
                int model_id;
                if (car_data)
                {
                    car_data += 4; // Skip "add="

                    // Extract the make_model value (e.g., "1:1")
                    char *make_model = strstr(car_data, "make_model=");
                    if (make_model)
                    {
                        make_model += 11;  // Skip "make_model=" part

                        // Now, split the make_model field by the colon ":"
                        char *make = strtok(make_model, "%3A");
                        if (make)
                        {
                            make_id = atoi(make);  // Convert to integer for Make ID
                            char *model = strtok(NULL, "%3A");
                            if (model)
                            {
                                model_id = atoi(model);  // Convert to integer for Model ID
                            }
                        }
                    }
                }

                printf("<p>POSTDATA:%s</p>",post_data);

                //get the fields
                int year = atoi(strstr(post_data, "year=") + 5);
                int mileage = atoi(strstr(post_data, "mileage=") + 8);
                double value = atof(strstr(post_data, "value=") + 6);
                int vin = atoi(strstr(post_data, "vin=") + 4);
                double mpg = atof(strstr(post_data, "mpg=") + 4);
                const char *license_plate = strstr(post_data, "license_plate=") + 15;
                const char *color = strstr(post_data, "color=") + 6;

                printf("<p>Color:%s</p>",color);
                printf("<p>Year:%d</p>",year);
                printf("<p>Make:%d</p>",make_id);
                printf("<p>Model:%d</p>",model_id);
                printf("<p>Value:%.2f</p>",value);
                printf("<p>Mileage:%d</p>",mileage);
                printf("<p>LicPlate:%s</p>",license_plate);
                printf("<p>MPG:%.1f</p>",mpg);
                printf("<p>VIN:%d</p>",vin);

                //perform the insertion
                sqlite3* db = Connect();
                sqlite3_stmt *stmt;
                char *query = //%Q escapes the strings (sql injection-proof)
                        "INSERT INTO Car (Color, Year, Make, Model, Value, Mileage, LicPlate, Miles_PerGal, Vin"
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
                int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                //bind the parameters
                sqlite3_bind_text(stmt, 1, color, (int) strlen(color), SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 2, year);
                sqlite3_bind_int(stmt, 3, make_id);
                sqlite3_bind_int(stmt, 4, model_id);
                sqlite3_bind_double(stmt, 5, value);
                sqlite3_bind_int(stmt, 6, mileage);
                sqlite3_bind_text(stmt, 7, license_plate, (int) strlen(license_plate), SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 8, mpg);
                sqlite3_bind_int(stmt, 9, vin);

                //close the statement
                sqlite3_step(stmt); //execute
                sqlite3_finalize(stmt);

                printf("<p>Car Successfully Added!%d</p>",rc);

                //add the record of the action to the record table


                // Query to get car make and models
                sqlite3_stmt *stmt2;
                const char *sql = "SELECT Make.Id, Model.Id, Make.Name, Model.Name FROM Make, Model WHERE Make.Id = Model.Make;";
                rc = sqlite3_prepare_v2(db, sql, -1, &stmt2, NULL);


                //regenerate the form
                printf("<h1 class=\"main_container\">Add a Car</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"AddCar\">\n");
                printf(" <input type=\"hidden\" name=\"action\" value=\"add\">\n");

                //make & model
                printf("  <label for=\"make\">Select a Make and Model:</label>\n");
                printf("<select style=\"width: 500px;\" id=\"make\" name=\"make_model\" required>\n");

                // Fetch and display car names as datalist options
                while (sqlite3_step(stmt2) == SQLITE_ROW)
                {
                    // Retrieve make and model IDs and their names
                    make_id = sqlite3_column_int(stmt2, 0);
                    model_id = sqlite3_column_int(stmt2, 1);
                    const char *make_name = (const char *)sqlite3_column_text(stmt2, 2);
                    const char *model_name = (const char *)sqlite3_column_text(stmt2, 3);

                    // Set the option value to be a combination of make_id and model_id, separated by a colon
                    printf("<option value=\"%d:%d\">%s %s</option>\n", make_id, model_id, make_name, model_name);
                }

                printf("</select><br>\n");

                //year
                printf("  <label for=\"year\">Year:</label>\n");
                printf("  <input type=\"number\" name=\"year\" id=\"year\" min=\"1900\" max=\"2025\" value=\"2025\" required><br>\n");

                //mileage
                printf("  <label for=\"mileage\">Mileage (in miles):</label>\n");
                printf("  <input type=\"number\" name=\"mileage\" id=\"mileage\" step=\"1\" value=\"10000\" required><br>\n");

                //value
                printf("  <label for=\"value\">Value (in dollars):</label>\n");
                printf("  <input type=\"number\" name=\"value\" id=\"value\" step=\"1\" value=\"15000\" required><br>\n");

                //vin
                printf("  <label for=\"vin\">VIN:</label>\n");
                printf("  <input type=\"text\" name=\"vin\" id=\"vin\" minlength=\"17\" maxlength=\"17\" required><br>\n");

                //mi/gal
                printf("  <label for=\"mpg\">Miles per Gallon:</label>\n");
                printf("  <input type=\"number\" name=\"mpg\" id=\"mpg\" step=\"0.1\" value=\"25\" required><br>\n");

                //lic plate digits
                printf("  <label for=\"license_plate\">License Plate:</label>\n");
                printf("  <input type=\"text\" name=\"license_plate\" id=\"license_plate\" minlength=\"6\" maxlength=\"6\" required><br>\n");

                //color
                printf("  <label for=\"color\">Car Color:</label>\n");
                printf("  <input type=\"text\" name=\"color\" id=\"color\" required><br>\n");

                //end of the form
                printf("  <input type=\"submit\" value=\"Add\">\n");
                printf("</form>\n");

                // Finalize and close
                sqlite3_finalize(stmt2);
                sqlite3_close(db);
            }
            else if (strncmp(action, "update", 6) == 0)
            {

            }


        }
    }

    return 0;
}

//function to create and return the database instance (struct pointer)
//will also create (if needed) the tables within the database
sqlite3* Connect()
{
    //open the database and place the result in the db variable which will be returned
    sqlite3* db;
    sqlite3_open("HD_Corp.db" , &db);

    //create the tables
    char* messageErr;
    sqlite3_exec(db,
       "CREATE TABLE IF NOT EXISTS Car(Id INTEGER PRIMARY KEY AUTOINCREMENT, Color TEXT NOT NULL, Year INTEGER NOT NULL, Make INTEGER NOT NULL, Model INTEGER NOT NULL, Value FLOAT NOT NULL, Mileage INTEGER NOT NULL, LicPlate VARCHAR(8) NOT NULL UNIQUE, Miles_PerGal INTEGER NOT NULL, Vin TEXT NOT NULL UNIQUE, FOREIGN KEY (Make) REFERENCES Make(Id), FOREIGN KEY (Model) REFERENCES Model(Id))"
       ,NULL, 0, &messageErr);
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS Make(Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL UNIQUE)"
       ,NULL, 0, &messageErr);
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS Model(Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL, Make INTEGER NOT NULL, FOREIGN KEY (Make) REFERENCES Make(Id))"
        ,NULL, 0, &messageErr);
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS Employee(Id INTEGER PRIMARY KEY AUTOINCREMENT, Username TEXT NOT NULL UNIQUE, Password VARCHAR(255) NOT NULL, Salt TEXT NOT NULL UNIQUE)"
        ,NULL, 0, &messageErr);
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS Record(Id INTEGER PRIMARY KEY AUTOINCREMENT, Employee INTEGER, Time DATETIME NOT NULL, ActionType TEXT NOT NULL, FOREIGN KEY (Employee) REFERENCES Employee(Id))"
        ,NULL, 0, &messageErr);

    return db;
}

//function callback for any count(*) queries that returns the count of that query
int callbackCount(void *NotUsed, int argc, char **Values,char **azColName)
{
    NotUsed = 0;

    return (int) Values[0]; //return the count of the query
}

//function to extract post data
void get_post_data(char *data, size_t size)
{
    if (getenv("CONTENT_LENGTH"))
    {
        int len = atoi(getenv("CONTENT_LENGTH"));
        if (len > 0 && len < size)
        {
            fread(data, 1, len, stdin);
            data[len] = '\0';
        }
    }
}

//function to read the post data
void read_post_data(char *data, int size)
{
    int content_length = atoi(getenv("CONTENT_LENGTH"));
    if (content_length > 0)
    {
        fgets(data, size, stdin);
    }
}

//function to decode url form data (to handle spaces and special chars)
void url_decode(char *src)
{
    char *dest = src;
    while (*src)
    {
        if (*src == '+')
        {  // Convert '+' to space
            *dest = ' ';
        }
        else if (*src == '%' && src[1] && src[2])
        {  // Decode %XX hex encoding
            char hex[3] = { src[1], src[2], '\0' };
            *dest = (char) strtol(hex, NULL, 16);
            src += 2;
        }
        else
        {
            *dest = *src;
        }
        dest++;
        src++;
    }
    *dest = '\0';
}

//function to add a record to the database
void AddRecord(sqlite3* db, char type[], int who)
{
    //get the current datetime
    time_t t = time(NULL);

    struct tm *tm_info = localtime(&t);

    char datetime_str[20];
    strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", tm_info);

    //prep the query
    char query[MAXLEN];
    snprintf(query, sizeof(query),
             "INSERT INTO Record (Employee, Time, ActionType) VALUES ('%d', '%s', '%s')",
             who, datetime_str, type);
    char* messageErr;

    //execute the query
    sqlite3_exec(db, query, 0, 0, &messageErr); //note we aren't mentioning recordings to the user(s)
}