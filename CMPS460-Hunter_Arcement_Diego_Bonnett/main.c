#include <stdio.h>
#include <sqlite3.h> //for sqlite database actions
#include <string.h>
#include <stdlib.h>
#include <time.h> //for getting timestamps for the record table
#include <sha256.h> //for hashing password + salt + pepper
#include <ctype.h>

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
void read_post_data(char *, int);
char* get_query_param(const char*, const char*);

//prototype to get rid of leading and trailing spaces
void strip_spaces(char *);

//converter for hex to bytes (hashing)
int hex_to_bytes(const char*, uint8_t*);

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

            // Extract employeeId from the query string
            if (query_string == NULL)
            {
                printf("<p>Error: No query string provided</p>\n");
                return 1;
            }

            // Extract employeeId using get_query_param function
            const char* employee_id = get_query_param(query_string, "employeeId");
            if (employee_id == NULL)
            {
                printf("<p>Error: No employeeId found in query string</p>\n");
                return 1;
            }

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
            printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST

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

                printf("    <option value=\"%d\">%s %s %s Valued at $%.2f VIN:%s Id:%d</option>\n",
                       sqlite3_column_int(stmt, 0),
                       sqlite3_column_text(stmt, 1),
                       sqlite3_column_text(stmt2, 0),
                       sqlite3_column_text(stmt2, 1),
                       sqlite3_column_double(stmt, 5),
                       sqlite3_column_text(stmt, 9),
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

            if (query_string == NULL)
            {
                printf("<p>Error: No query string provided</p>\n");
                return 1;
            }

            // Extract employeeId from the query string
            const char* employee_id = get_query_param(query_string, "employeeId");

            if (employee_id == NULL)
            {
                printf("<p>Error: No employeeId found in query string</p>\n");
                return 1;
            }

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
            printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST

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
            printf("  <input type=\"text\" name=\"vin\" id=\"vin\" minlength=\"17\" maxlength=\"17\" pattern=\"[0-9]+\" required><br>\n");

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
            //print header info
            printf("Content-type: text/html\n\n");

            //generate the form
            printf("<h1 class=\"main_container\">Employee Sign In</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"SignIn\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"SignIn\">\n");

            //username
            printf("<label for=\"username\" >Username </label>");
            printf("<input type=\"text\" name=\"username\" id=\"username\" required>\n");

            //password
            printf("<br><label for=\"pass\" >Password </label>");
            printf("<input type=\"password\" name=\"password\" id=\"pass\" required>\n");

            printf("<br><input type=\"submit\" value=\"Sign In\">\n");
            printf("</form>\n");
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
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Cars by Color</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ByColor\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ByColor\">\n");

            //color
            printf("<label for=\"color\">Color to Search By:</label>\n");
            //fetch and display car names as datalist options
            printf("<select style=\"width: 500px;\" id=\"color\" name=\"color\" required>\n");

            sqlite3 *db = Connect();
            sqlite3_stmt *stmt;
            char sql[] = "SELECT Color FROM Car";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                printf("<option value=\"%s\">%s</option>\n",
                       sqlite3_column_text(stmt, 0),
                       sqlite3_column_text(stmt, 0));
            }

            printf("</select>\n");

            printf("<br><input type=\"submit\" value=\"List Cars\">\n");
            printf("</form>\n");

            //close the database and finalize the statement
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        //ListByMake.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMake"))
        {

        }
        //ListByMileage.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMileage"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Cars by Mileage</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ByMileage\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ByMileage\">\n");

            //year number
            printf("<label for=\"mileage\">Mileage to Search By:</label>\n");
            printf("<input type=\"number\"  name=\"mileage\"  id=\"mileage\" step=\"1\" value=\"50000\" required><br>\n");

            //less than or greater than (value number)
            printf("<p>Greater Than or Less Than the given value:</p>");
            printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
            printf("<label for=\"greater\" >Greater Than</label>");
            printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
            printf("<label for=\"lesser\">Less Than</label>");

            printf("<br><input type=\"submit\" value=\"List Cars\">\n");
            printf("</form>\n");
        }
        //ListByMilePerGal.html GET logic
        else if (query_string && strstr(query_string, "page=ListByMilePerGal"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Cars by Mile Per Gallon</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ByMpg\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ByMpg\">\n");

            //year number
            printf("<label for=\"mileage\">Mi/Gal to Search By:</label>\n");
            printf("<input type=\"number\"  name=\"mpg\"  id=\"mpg\" step=\"1\" value=\"20.0\" required><br>\n");

            //less than or greater than (value number)
            printf("<p>Greater Than or Less Than the given value:</p>");
            printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
            printf("<label for=\"greater\" >Greater Than</label>");
            printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
            printf("<label for=\"lesser\">Less Than</label>");

            printf("<br><input type=\"submit\" value=\"List Cars\">\n");
            printf("</form>\n");
        }
        //ListByModel.html GET logic
        else if (query_string && strstr(query_string, "page=ListByModel"))
        {  

        }
        //ListByValue.html GET logic
        else if (query_string && strstr(query_string, "page=ListByValue"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Cars by Value</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ByValue\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ByValue\">\n");

            //year number
            printf("<label for=\"value\">Value to Search By:</label>\n");
            printf("<input type=\"number\"  name=\"value\"  id=\"value\" step=\"1\" value=\"15000\" required><br>\n");

            //less than or greater than (value number)
            printf("<p>Greater Than or Less Than the given value:</p>");
            printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
            printf("<label for=\"greater\" >Greater Than</label>");
            printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
            printf("<label for=\"lesser\">Less Than</label>");

            printf("<br><input type=\"submit\" value=\"List Cars\">\n");
            printf("</form>\n");
        }
        //ListByYear.html GET logic
        else if (query_string && strstr(query_string, "page=ListByYear"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Cars by Year</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ByYear\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ByYear\">\n");

            //year number
            printf("<label for=\"year\">Year to Search By:</label>\n");
            printf("<input type=\"number\"  name=\"year\"  id=\"year\" step=\"1\" value=\"2025\" required><br>\n");

            //less than or greater than (value number)
            printf("<p>Greater Than or Less Than the given year:</p>");
            printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
            printf("<label for=\"greater\" >Greater Than</label>");
            printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
            printf("<label for=\"lesser\">Less Than</label>");

            printf("<br><input type=\"submit\" value=\"List Cars\">\n");
            printf("</form>\n");
        }
        //ListModelByMake.html GET logic
        else if (query_string && strstr(query_string, "page=ListModelByMake"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Generate the form
            printf("<h1 class=\"main_container\">List Model(s) by Make</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"ModelByMake\">\n");
            printf("<input type=\"hidden\" name=\"action\" value=\"ModelByMake\">\n");

            //makes
            printf("<label for=\"make\">Make to Search By:</label>\n");
            //fetch and display car names as datalist options
            printf("<select style=\"width: 500px;\" id=\"make\" name=\"make\" required>\n");

            sqlite3 *db = Connect();
            sqlite3_stmt *stmt;
            char sql[] = "SELECT * FROM Make";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                printf("<option value=\"%d\">%s</option>\n",
                       sqlite3_column_int(stmt, 0),
                       sqlite3_column_text(stmt, 1));
            }

            printf("</select>\n");

            printf("<br><input type=\"submit\" value=\"List Models\">\n");
            printf("</form>\n");

            //close the database and finalize the statement
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        //UpdateCar.html GET logic
        else if (query_string && strstr(query_string, "page=UpdateCarMain"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Extract employeeId from the query string
            if (query_string == NULL)
            {
                printf("<p>Error: No query string provided</p>\n");
                return 1;
            }

            // Extract employeeId from the query string
            const char* employee_id = get_query_param(query_string, "emp_Id");
            if (employee_id == NULL)
            {
                printf("<p>Error: No employeeId found in query string</p>\n");
                return 1;
            }

            //extract the car id as well
            const char* car_id = get_query_param(query_string, "car");
            if (car_id == NULL)
            {
                printf("<p>Error: No car id found in query string</p>\n");
                return 1;
            }

            //prepare the query(ies) for the form generation
            sqlite3 *db = Connect();
            sqlite3_stmt *stmt;
            char sql[] = "SELECT * FROM Car WHERE Id = ?";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            sqlite3_bind_int(stmt, 1, atoi(car_id));
            sqlite3_step(stmt); //the row contains the initial car data

            sqlite3_stmt *stmt2;
            char sql2[] = "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = ? AND Model.Id = ?";
            sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL);
            sqlite3_bind_int(stmt2, 1, sqlite3_column_int(stmt, 3));
            sqlite3_bind_int(stmt2, 2, sqlite3_column_int(stmt, 4));
            sqlite3_step(stmt2); //the row contains the initial car make and model names

            sqlite3_stmt *stmt3;
            char sql3[] = "SELECT Make.Name, Model.Name, Make.Id, Model.Id FROM Make, Model WHERE Make.Id != ? AND Model.Id != ? AND Model.Make = Make.Id";
            sqlite3_prepare_v2(db, sql3, -1, &stmt3, NULL);
            sqlite3_bind_int(stmt3, 1, sqlite3_column_int(stmt, 3));
            sqlite3_bind_int(stmt3, 2, sqlite3_column_int(stmt, 4));

            //generate the form
            //NOTE THAT THE ID, VIN, AND LIC PLATE CAN'T BE UPDATED (INTENDED)
            printf("<h1 class=\"main_container\">Update a Car</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"UpdateCarMain\">\n");
            printf(" <input type=\"hidden\" name=\"action\" value=\"UpdateFinal\">\n");
            printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST
            printf(" <input type=\"hidden\" name=\"car_id\" value=\"%s\">\n", car_id); //send the car id in the POST

            //make & model
            printf("  <label for=\"make\">Make and Model:</label>\n");
            printf("<select style=\"width: 500px;\" id=\"make\" name=\"make_model\" required>\n");

            //add the current car make and model first (default)
            printf("<option value=\"%d:%d\">%s %s</option>\n", sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1));

            //fetch and display car names as datalist options
            while (sqlite3_step(stmt3) == SQLITE_ROW)
            {
                //retrieve make and model IDs and their names
                int make_id = sqlite3_column_int(stmt3, 2);
                int model_id = sqlite3_column_int(stmt3, 3);
                const char *make_name = (const char *)sqlite3_column_text(stmt3, 0);
                const char *model_name = (const char *)sqlite3_column_text(stmt3, 1);

                //set the option value to be a combination of make_id and model_id, separated by a colon
                printf("<option value=\"%d:%d\">%s %s</option>\n", make_id, model_id, make_name, model_name);
            }

            printf("</select><br>\n");

            //year
            printf("  <label for=\"year\">Year:</label>\n");
            printf("  <input type=\"number\" name=\"year\" id=\"year\" min=\"1900\" max=\"2025\" value=\"%d\" required><br>\n",
                   sqlite3_column_int(stmt, 2));

            //mileage
            printf("  <label for=\"mileage\">Mileage (in miles):</label>\n");
            printf("  <input type=\"number\" name=\"mileage\" id=\"mileage\" step=\"1\" value=\"%d\" required><br>\n",
                   sqlite3_column_int(stmt, 6));

            //value
            printf("  <label for=\"value\">Value (in dollars):</label>\n");
            printf("  <input type=\"number\" name=\"value\" id=\"value\" step=\"1\" value=\"%.2f\" required><br>\n",
                   sqlite3_column_double(stmt, 5));

            //mi/gal
            printf("  <label for=\"mpg\">Miles per Gallon:</label>\n");
            printf("  <input type=\"number\" name=\"mpg\" id=\"mpg\" step=\"0.1\" value=\"%.2f\" required><br>\n",
                   sqlite3_column_double(stmt, 8));

            //color
            printf("  <label for=\"color\">Car Color:</label>\n");
            printf("  <input type=\"text\" name=\"color\" id=\"color\" value=\"%s\" required><br>\n",
                   sqlite3_column_text(stmt, 1));

            //end of the form
            printf("  <input type=\"submit\" value=\"Update\">\n");
            printf("</form>\n");

            //close the database connection and all the statements
            sqlite3_finalize(stmt);
            sqlite3_finalize(stmt2);
            sqlite3_finalize(stmt3);
            sqlite3_close(db);
        }
        //UpdateCarDecide.html GET logic
        else if (query_string && strstr(query_string, "page=UpdateCarDecide"))
        {
            //print header info
            printf("Content-type: text/html\n\n");

            // Extract employeeId from the query string
            if (query_string == NULL)
            {
                printf("<p>Error: No query string provided</p>\n");
                return 1;
            }

            // Extract employeeId from the query string
            const char* employee_id = get_query_param(query_string, "employeeId");
            if (employee_id == NULL)
            {
                printf("<p>Error: No employeeId found in query string</p>\n");
                return 1;
            }

            sqlite3* db = Connect(); // Connect to the database
            sqlite3_stmt *stmt;
            int rc;

            // Query to get cars
            const char *sql = "SELECT * FROM Car;";
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            if (rc != SQLITE_OK)
            {
                printf("<p>Error preparing query: %s</p>\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                return 1;
            }

            // Generate the form
            printf("<h1 class=\"main_container\">Choose a Car to Update</h1>\n");
            printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
            // Hidden input to pass the page context
            printf("<input type=\"hidden\" name=\"page\" value=\"updateDecide\">\n");
            printf(" <input type=\"hidden\" name=\"action\" value=\"updateDecide\">\n");
            printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST

            printf("  <label for=\"car\">Select Car to Update:</label>\n");

            // Fetch and display car names as datalist options
            printf("<select style=\"width: 500px;\" id=\"car\" name=\"car\" required>\n");
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                char query[MAXLEN];
                snprintf(query, sizeof(query),
                         "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                         sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                sqlite3_stmt *stmt2;
                sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                sqlite3_step(stmt2);

                printf("    <option value=\"%d\">%s %s %s Valued at $%.2f VIN:%s Id:%d</option>\n",
                       sqlite3_column_int(stmt, 0),
                       sqlite3_column_text(stmt, 1),
                       sqlite3_column_text(stmt2, 0),
                       sqlite3_column_text(stmt2, 1),
                       sqlite3_column_double(stmt, 5),
                       sqlite3_column_text(stmt, 9),
                       sqlite3_column_int(stmt, 0));
            }

            printf("</select>\n");
            printf("<input type=\"submit\" value=\"Update\">\n");
            printf("</form>\n");

            sqlite3_finalize(stmt);
            sqlite3_close(db);
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

        char post_data[MAXLEN];
        read_post_data(post_data, MAXLEN);
        char post_data_2[MAXLEN];
        strcpy(post_data_2, post_data);
        char post_data_3[MAXLEN];
        strcpy(post_data_3, post_data);

        //determine the action
        char *action = strstr(post_data, "action=");
        if (action)
        {
            action += 7;
            if (strncmp(action, "remove", 6) == 0)
            {
                printf("Content-type: text/html\n\n");

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
                    int result = sqlite3_exec(db, query, NULL ,0, &errMssg);

                    char* employee_id = strstr(post_data_3, "emp_id=");  // post_data_3 contains full POST string
                    if (employee_id)
                    {
                        employee_id += 7;  // Skip "emp_id="
                        char* amp = strchr(employee_id, '&');
                        if (amp) *amp = '\0';  // Null-terminate if there's another param after
                    }
                    else
                    {
                        printf("<p>Error: No emp_id found in POST data</p>\n");
                        return 1;
                    }

                    //add the record to the database (still need the id of the employee from POST)
                    AddRecord(db, "Removed Car", atoi(employee_id));

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
                    printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST

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

                        printf("    <option value=\"%d\">%s %s %s Valued at $%.2f VIN:%s Id:%d</option>\n",
                               sqlite3_column_int(stmt, 0),
                               sqlite3_column_text(stmt, 1),
                               sqlite3_column_text(stmt2, 0),
                               sqlite3_column_text(stmt2, 1),
                               sqlite3_column_double(stmt, 5),
                               sqlite3_column_text(stmt, 9),
                               sqlite3_column_int(stmt, 0));
                    }

                    printf("</select>\n");
                    printf("  <input type=\"submit\" value=\"remove\">\n");
                    printf("</form>\n");

                    sqlite3_finalize(stmt);
                    sqlite3_close(db);

                    if (result != SQLITE_OK) //if the result int was a not ok, declare to the user that there was an error inserting the entry
                    {
                        printf("<p>Deletion Error</p>");
                    }
                    else if(result == SQLITE_OK)
                    {
                        printf("<p>Deletion Successful</p>");
                    }
                }
            }
            else if (strncmp(action, "add", 3) == 0)
            {
                printf("Content-type: text/html\n\n");

                char *car_data = strstr(post_data_2, "add");
                int make_id;
                int model_id;
                if (car_data)
                {
                    car_data += 4; // Skip "add="

                    //extract the make_model value (e.g., "1:1")
                    char *make_model = strstr(car_data, "make_model=");
                    if (make_model)
                    {
                        make_model += 11;  // Skip "make_model=" part

                        //split the make_model field by the colon ":"
                        char *make = strtok(make_model, "%3A");
                        if (make)
                        {
                            make_id = atoi(make);  //convert to integer for Make ID
                            char *model = strtok(NULL, "%3A");
                            if (model)
                            {
                                model_id = atoi(model);  //convert to integer for Model ID
                            }
                        }
                    }
                }

                //extract the other fields
                int year = atoi(strstr(post_data_3, "year=") + 5);
                int mileage = atoi(strstr(post_data_3, "mileage=") + 8);
                float value = atof(strstr(post_data_3, "value=") + 6);
                char *vin = strstr(post_data_3, "vin=") + 4;
                double mpg = atof(strstr(post_data_3, "mpg=") + 4);
                char *license_plate = strstr(post_data_3, "license_plate=") + 15;
                const char *color = strstr(post_data_3, "color=") + 6;

                //additional handling for the license_plate
                char *delimiter_pos = strchr(license_plate, '&'); //get where the & is
                if (delimiter_pos != NULL)
                {
                    // Copy the substring up to the delimiter
                    size_t length = delimiter_pos - license_plate; // Length of substring up to delimiter
                    strncpy(license_plate, license_plate, length);  // Copy the substring into the output
                    license_plate[length] = '\0';         // Null-terminate the output string
                } else
                {
                    // If no delimiter is found, copy the whole string
                    strcpy(license_plate, license_plate);
                }

                //additional handling for the vin
                delimiter_pos = strchr(vin, '&'); //get where the & is
                if (delimiter_pos != NULL)
                {
                    // Copy the substring up to the delimiter
                    size_t length = delimiter_pos - vin; // Length of substring up to delimiter
                    strncpy(vin, vin, length);  // Copy the substring into the output
                    vin[length] = '\0';         // Null-terminate the output string
                } else
                {
                    // If no delimiter is found, copy the whole string
                    strcpy(vin, vin);
                }

                //perform the insertion
                sqlite3* db = Connect();
                sqlite3_stmt *stmt;
                char *query = //%Q escapes the strings (sql injection-proof)
                        "INSERT INTO Car (Color, Year, Make, Model, Value, Mileage, LicPlate, Miles_PerGal, Vin)"
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
                sqlite3_bind_text(stmt, 9, vin, (int) strlen(vin), SQLITE_TRANSIENT);

                //close the statement
                sqlite3_step(stmt); //execute
                sqlite3_finalize(stmt);

                if (rc != SQLITE_OK) //if the result int was a not ok, declare to the user that there was an error inserting the entry
                {
                    printf("<p>Insertion Error</p>");
                }
                else if(rc == SQLITE_OK)
                {
                    printf("<p>Insertion Successful</p>");
                }

                char* employee_id = strstr(post_data_3, "emp_id=");  // post_data_3 contains full POST string
                if (employee_id)
                {
                    employee_id += 7;  // Skip "emp_id="
                    char* amp = strchr(employee_id, '&');
                    if (amp) *amp = '\0';  // Null-terminate if there's another param after
                }
                else
                {
                    printf("<p>Error: No emp_id found in POST data</p>\n");
                    return 1;
                }

                //add the record of the action to the record table
                AddRecord(db, "Added Car", atoi(employee_id));

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
                printf(" <input type=\"hidden\" name=\"emp_id\" value=\"%s\">\n", employee_id); //send the employee id in the POST

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
                printf("  <input type=\"text\" name=\"vin\" id=\"vin\" minlength=\"17\" maxlength=\"17\" pattern=\"[0-9]+\" required><br>\n");

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
            else if (strncmp(action, "SignIn", 6) == 0)
            {
                char *employee = strstr(post_data_2, "username");
                if (employee)
                {
                    printf("Content-type: text/html\n\n");

                    //get the username and password information from the POST data
                    char *username = strstr(post_data_3, "username=") + 9;

                    //additional handling for the username
                    char *delimiter_pos = strchr(username, '&'); //get where the & is
                    if (delimiter_pos != NULL)
                    {
                        // Copy the substring up to the delimiter
                        size_t length = delimiter_pos - username; // Length of substring up to delimiter
                        strncpy(username, username, length);  // Copy the substring into the output
                        username[length] = '\0';         // Null-terminate the output string
                    }
                    else
                    {
                        // If no delimiter is found, copy the whole string
                        strcpy(username, username);
                    }

                    strip_spaces(username);

                    char *password = strstr(post_data_2, "password=") + 9;

                    //see if the username exists in the database, if so get the salt (for the hash check)
                    //if not then reprint the page with the user feedback message
                    sqlite3 *db = Connect(); //connect to the database

                    sqlite3_stmt *stmtC;
                    char sqlC[] = "SELECT COUNT(*) FROM Employee WHERE Username = ?";
                    int rcC = sqlite3_prepare_v2(db, sqlC, -1, &stmtC, NULL);
                    sqlite3_bind_text(stmtC, 1, username, (int) strlen(username), SQLITE_TRANSIENT);
                    sqlite3_step(stmtC);
                    int count = sqlite3_column_int(stmtC, 0);
                    sqlite3_finalize(stmtC);

                    sqlite3_stmt *stmt;
                    char sql[] = "SELECT * FROM Employee WHERE Username = ?";
                    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                    sqlite3_bind_text(stmt, 1, username, (int) strlen(username), SQLITE_TRANSIENT);

                    rc = sqlite3_step(stmt);
                    if (count == 0) //username did not exist
                    {
                        //user feedback
                        printf("<p>Username or Password invalid, please ensure your username and password are correct.</p>\n");

                        //generate the form
                        printf("<h1 class=\"main_container\">Employee Sign In</h1>\n");
                        printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                        // Hidden input to pass the page context
                        printf("<input type=\"hidden\" name=\"page\" value=\"SignIn\">\n");
                        printf("<input type=\"hidden\" name=\"action\" value=\"SignIn\">\n");

                        //username
                        printf("<label for=\"username\" >Username </label>");
                        printf("<input type=\"text\" name=\"username\" id=\"username\" required>\n");

                        //password
                        printf("<br><label for=\"pass\" >Password </label>");
                        printf("<input type=\"password\" name=\"password\" id=\"pass\" required>\n");

                        printf("<br><input type=\"submit\" value=\"Sign In\">\n");
                        printf("</form>\n");
                    }
                    //get the salt (text in column index 3), then assemble the password + salt + pepper
                    //pepper is HRFWWTAP (Hunter Runs From Women When They Approach Him)
                    char HashMe[1024]; // Ensure this buffer is large enough for the concatenated string
                    snprintf(HashMe, sizeof(HashMe), "%s%sHRFWWTAH", password, sqlite3_column_text(stmt, 3));
                    SHA256_CTX ctx;
                    uint8_t hash[SHA256_BLOCK_SIZE];
                    sha256_init(&ctx);
                    sha256_update(&ctx, (uint8_t *) HashMe, strlen(HashMe));
                    sha256_final(&ctx, hash);

                    //figure out if the username-password combination are correct
                    //if not, reprint the page with the user feedback message
                    // Retrieve the hash from the database (example: hex string)
                    const char *stored_hash_hex = sqlite3_column_text(stmt, 2);
                    uint8_t stored_hash[SHA256_BLOCK_SIZE];

                    // Convert the stored hex string to a byte array
                    if (hex_to_bytes(stored_hash_hex, stored_hash) != 0)
                    {
                        printf("Error: Invalid hexadecimal string.\n");
                        return 1;
                    }

                    // Compare the generated hash with the stored hash
                    if (memcmp(hash, stored_hash, SHA256_BLOCK_SIZE) == 0)
                    {
                        //generate the new form (get what action this employee wants to do)
                        //get the employee id for the POST to the other pages
                        int id = sqlite3_column_int(stmt, 0);
                        sqlite3_finalize(stmt);

                        //generate the form
                        printf("<h1 class=\"main_container\">Employee Sign In</h1>\n");
                        printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                        // Hidden input to pass the page context
                        printf("<input type=\"hidden\" name=\"page\" value=\"perform\">\n");
                        printf("<input type=\"hidden\" name=\"action\" value=\"SignIn\">\n");
                        printf("<input type=\"hidden\" name=\"emp_id\" value=\"%d\">\n", id); //employee's id being passed to next screen

                        //actions
                        printf("<p>Select Action \'%s\':</p>", username);
                        printf("<input type=\"radio\" name=\"userAction\" id=\"remove\" value=\"remove\" required>");
                        printf("<label for=\"remove\" >Remove Car From Database</label>");
                        printf("<input type=\"radio\" name=\"userAction\" id=\"add\" value=\"add\" required>");
                        printf("<label for=\"add\">Add Car To Database</label>");
                        printf("<input type=\"radio\" name=\"userAction\" id=\"update\" value=\"update\" required>");
                        printf("<label for=\"update\">Update Car In Database</label>");

                        printf("<br><input type=\"submit\" value=\"Perform\">\n");
                        printf("</form>\n");
                    }
                    else
                    {
                        //user feedback
                        printf("<p>Username or Password invalid, please ensure your username and password are correct. DEBUG=2</p>\n");

                        //generate the form
                        printf("<h1 class=\"main_container\">Employee Sign In</h1>\n");
                        printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                        // Hidden input to pass the page context
                        printf("<input type=\"hidden\" name=\"page\" value=\"SignIn\">\n");
                        printf("<input type=\"hidden\" name=\"action\" value=\"SignIn\">\n");

                        //username
                        printf("<input type=\"text\" id=\"username\" value=\"username\" required>\n");
                        printf("<label for=\"username\" >Username </label>");

                        //password
                        printf("<input type=\"password\" id=\"pass\" value=\"pass\" required>\n");
                        printf("<label for=\"pass\" >Password </label>");

                        printf("<br><input type=\"submit\" value=\"Sign In\">\n");
                        printf("</form>\n");
                    }
                }
                else //for this section we are dealing with POST from the second form (get the info and redirect via POST)
                {
                    //get the value of the radio button AND the employee id from the POST
                    int emp_id = 0;
                    char *emp_start = strstr(post_data_3, "emp_id=");
                    if (emp_start != NULL)
                    {
                        emp_start += 7; //length of "emp_id="
                        emp_id = atoi(emp_start);
                    }

                    char rawAction[100];
                    char *start = strstr(post_data_3, "userAction=");
                    if (start != NULL)
                    {
                        start += 11; // length of "userAction="
                        char *end = strchr(start, '&');
                        if (end != NULL)
                        {
                            size_t len = end - start;
                            if (len >= sizeof(rawAction)) len = sizeof(rawAction) - 1;
                            strncpy(rawAction, start, len);
                            rawAction[len] = '\0';
                        }
                        else
                        {
                            strncpy(rawAction, start, sizeof(rawAction) - 1);
                            rawAction[sizeof(rawAction) - 1] = '\0';
                        }
                    }

                    //prep for the redirect
                    char redirect_url[MAXLEN] = "";;
                    if (strcmp(rawAction, "add") == 0)
                    {
                        snprintf(redirect_url, sizeof(redirect_url), "/cgi-bin/HD_Corp.exe?page=AddCar&employeeId=%d", emp_id);
                    }
                    else if (strcmp(rawAction, "remove") == 0)
                    {
                        snprintf(redirect_url, sizeof(redirect_url), "/cgi-bin/HD_Corp.exe?page=RemoveCar&employeeId=%d", emp_id);
                    }
                    else if (strcmp(rawAction, "update") == 0)
                    {
                        snprintf(redirect_url, sizeof(redirect_url), "/cgi-bin/HD_Corp.exe?page=UpdateCarDecide&employeeId=%d", emp_id);
                    }

                    // Set status code and Location header for redirection
                    printf("Status: 302 Found\n");
                    printf("Location: %s\r\n", redirect_url);
                    printf("Content-Type: text/html\r\n");
                    printf("\r\n");
                }
            }
            else if (strncmp(action, "ByValue", 7) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ByValue");
                if (car_data)
                {
                    car_data += 8; //skip "ByValue="

                    double value = atof(strstr(post_data_3, "value=") + 6);
                    char *direction = strstr(post_data_3, "direction=") + 10;

                    //additional handling for the direction
                    char *delimiter_pos = strchr(direction, '&'); //get where the & is
                    if (delimiter_pos != NULL)
                    {
                        // Copy the substring up to the delimiter
                        size_t length = delimiter_pos - direction; // Length of substring up to delimiter
                        strncpy(direction, direction, length);  // Copy the substring into the output
                        direction[length] = '\0';         // Null-terminate the output string
                    }
                    else
                    {
                        // If no delimiter is found, copy the whole string
                        strcpy(direction, direction);
                    }

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char *query;
                    if(strncmp(direction, "GT", strlen(direction)) == 0)
                    {
                        query = "SELECT * FROM Car WHERE Value > ?";
                    }
                    else
                    {
                        query = "SELECT * FROM Car WHERE Value < ?";
                    }
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the value
                    sqlite3_bind_double(stmt, 1, value);

                    //iterate through each step (and also get the names of the make and model while doing so!
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        char query[MAXLEN];
                        snprintf(query, sizeof(query),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("<p>%s %s %s %d, Valued at: $%.2f With: %.2fmi/Gal With:%d License Plate:%s VIN:%s</p>\n"
                                , sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1)
                                , sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 8)
                                , sqlite3_column_int(stmt, 6), sqlite3_column_text(stmt, 7), sqlite3_column_text(stmt, 9));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Cars Found</p>");
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Cars by Value</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ByValue\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ByValue\">\n");

                //year number
                printf("<label for=\"value\">Value to Search By:</label>\n");
                printf("<input type=\"number\" name=\"value\" id=\"value\" step=\"1\" value=\"15000\" required><br>\n");

                //less than or greater than (value number)
                printf("<p>Greater Than or Less Than the given value:</p>");
                printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
                printf("<label for=\"greater\" >Greater Than</label>");
                printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
                printf("<label for=\"lesser\">Less Than</label>");

                printf("<br><input type=\"submit\" value=\"List Cars\">\n");
                printf("</form>\n");
            }
            else if (strncmp(action, "updateDecide", 7) == 0)
            {
                //get the car id and the employee id from the POST
                int carId = atoi(strstr(post_data_2, "car=") + 4);

                //extract employeeId using get_query_param function
                char* employee_id = strstr(post_data_3, "emp_id=");  // post_data_3 contains full POST string
                if (employee_id)
                {
                    employee_id += 7;  // Skip "emp_id="
                    char* amp = strchr(employee_id, '&');
                    if (amp) *amp = '\0';  // Null-terminate if there's another param after
                }
                else
                {
                    printf("Content-type: text/html\n\n");
                    printf("<p>Error: No emp_id found in POST data</p>\n");
                    return 1;
                }
                int emp_id = atoi(employee_id);

                //redirect to the UpdateCar.html via a GET
                char redirect_url[MAXLEN] = "";
                snprintf(redirect_url, sizeof(redirect_url), "/cgi-bin/HD_Corp.exe?page=UpdateCarMain&emp_Id=%d&car=%d", emp_id, carId);

                // Set status code and Location header for redirection
                printf("Status: 302 Found\n");
                printf("Location: %s\r\n", redirect_url);
                printf("Content-Type: text/html\r\n");
                printf("\r\n");
            }
            else if (strncmp(action, "UpdateFinal", 7) == 0)
            {
                char *car_data = strstr(post_data_2, "UpdateFinal");
                int make_id;
                int model_id;
                if (car_data)
                {
                    car_data += 13; // Skip "UpdateFinal="

                    //extract the make_model value (e.g., "1:1")
                    char *make_model = strstr(car_data, "make_model=");
                    if (make_model)
                    {
                        make_model += 11;  // Skip "make_model=" part

                        //split the make_model field by the colon ":"
                        char *make = strtok(make_model, "%3A");
                        if (make)
                        {
                            make_id = atoi(make);  //convert to integer for Make ID
                            char *model = strtok(NULL, "%3A");
                            if (model)
                            {
                                model_id = atoi(model);  //convert to integer for Model ID
                            }
                        }
                    }
                }

                //extract the other fields
                int year = atoi(strstr(post_data_3, "year=") + 5);
                int mileage = atoi(strstr(post_data_3, "mileage=") + 8);
                float value = atof(strstr(post_data_3, "value=") + 6);
                double mpg = atof(strstr(post_data_3, "mpg=") + 4);
                const char *color = strstr(post_data_3, "color=") + 6;

                //get the car id and employee if from the post data
                char* car_id = strstr(post_data_3, "car_id=");  // post_data_3 contains full POST string
                if (car_id)
                {
                    car_id += 7;  // Skip "car_id="
                    char* amp = strchr(car_id, '&');
                    if (amp) *amp = '\0';  // Null-terminate if there's another param after
                }
                else
                {
                    printf("Content-type: text/html\n\n");
                    printf("<p>Error: No car_id found in POST data</p>\n");
                    return 1;
                }


                char* employee_id = strstr(post_data_3, "emp_id=");  // post_data_3 contains full POST string
                if (employee_id)
                {
                    employee_id += 7;  // Skip "emp_id="
                    char* amp = strchr(employee_id, '&');
                    if (amp) *amp = '\0';  // Null-terminate if there's another param after
                }
                else
                {
                    printf("Content-type: text/html\n\n");
                    printf("<p>Error: No emp_id found in POST data</p>\n");
                    return 1;
                }

                //perform the update
                sqlite3 *db = Connect();
                sqlite3_stmt *stmt;
                char sql[] = "UPDATE Car SET Make = ?, Model = ?, Year = ?, Color = ?, Value = ?, Mileage = ?, Miles_PerGal = ? WHERE Id = ?";
                sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                sqlite3_bind_int(stmt, 1, make_id);
                sqlite3_bind_int(stmt, 2, model_id);
                sqlite3_bind_int(stmt, 3, year);
                sqlite3_bind_text(stmt, 4, color, (int) strlen(color), SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 5, value);
                sqlite3_bind_int(stmt, 6, mileage);
                sqlite3_bind_double(stmt, 7, mpg);
                sqlite3_bind_int(stmt, 8, atoi(car_id));
                sqlite3_step(stmt);


                //record the action
                AddRecord(db, "Update Car", atoi(employee_id));

                //close the database connection and finalize statements
                sqlite3_finalize(stmt);
                sqlite3_close(db);

                //redirect to the update decision page with just the emp_id
                char redirect_url[MAXLEN] = "";
                snprintf(redirect_url, sizeof(redirect_url), "/cgi-bin/HD_Corp.exe?page=UpdateCarDecide&employeeId=%d", atoi(employee_id));

                // Set status code and Location header for redirection
                printf("Status: 302 Found\n");
                printf("Location: %s\r\n", redirect_url);
                printf("Content-Type: text/html\r\n");
                printf("\r\n");
            }
            else if (strncmp(action, "ByYear", 6) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ByYear");
                if (car_data)
                {
                    car_data += 6; //skip "ByValue="

                    int value = atoi(strstr(post_data_3, "year=") + 5);
                    char *direction = strstr(post_data_3, "direction=") + 10;

                    //additional handling for the direction
                    char *delimiter_pos = strchr(direction, '&'); //get where the & is
                    if (delimiter_pos != NULL)
                    {
                        // Copy the substring up to the delimiter
                        size_t length = delimiter_pos - direction; // Length of substring up to delimiter
                        strncpy(direction, direction, length);  // Copy the substring into the output
                        direction[length] = '\0';         // Null-terminate the output string
                    }
                    else
                    {
                        // If no delimiter is found, copy the whole string
                        strcpy(direction, direction);
                    }

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char *query;
                    if(strncmp(direction, "GT", strlen(direction)) == 0)
                    {
                        query = "SELECT * FROM Car WHERE Year > ?";
                    }
                    else
                    {
                        query = "SELECT * FROM Car WHERE Year < ?";
                    }
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the value
                    sqlite3_bind_int(stmt, 1, value);

                    //iterate through each step (and also get the names of the make and model while doing so!
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        char query[MAXLEN];
                        snprintf(query, sizeof(query),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("<p>%s %s %s %d, Valued at: $%.2f With: %.2fmi/Gal With:%d License Plate:%s VIN:%s</p>\n"
                                , sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1)
                                , sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 8)
                                , sqlite3_column_int(stmt, 6), sqlite3_column_text(stmt, 7), sqlite3_column_text(stmt, 9));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Cars Found</p>");
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Cars by Year</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ByYear\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ByYear\">\n");

                //year number
                printf("<label for=\"year\">Year to Search By:</label>\n");
                printf("<input type=\"number\" name=\"year\" id=\"year\" step=\"1\" value=\"2025\" required><br>\n");

                //less than or greater than (value number)
                printf("<p>Greater Than or Less Than the given year:</p>");
                printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
                printf("<label for=\"greater\" >Greater Than</label>");
                printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
                printf("<label for=\"lesser\">Less Than</label>");

                printf("<br><input type=\"submit\" value=\"List Cars\">\n");
                printf("</form>\n");
            }
            else if (strncmp(action, "ByColor", 7) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ByColor");
                if (car_data)
                {
                    car_data += 8; //skip "ByValue="

                    char *color = strstr(post_data_3, "color=") + 6;

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char query[] = "SELECT * FROM Car WHERE Color = ?";
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the value
                    sqlite3_bind_text(stmt, 1, color, strlen(color), SQLITE_TRANSIENT);

                    //iterate through each step (and also get the names of the make and model while doing so!
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        char query2[MAXLEN];
                        snprintf(query2, sizeof(query2),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query2, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("<p>%s %s %s %d, Valued at: $%.2f With: %.2fmi/Gal With:%d License Plate:%s VIN:%s</p>\n"
                                , sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1)
                                , sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 8)
                                , sqlite3_column_int(stmt, 6), sqlite3_column_text(stmt, 7), sqlite3_column_text(stmt, 9));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Cars Found</p>");

                    //close the database and finalize the statement
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Cars by Color</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ByColor\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ByColor\">\n");

                //color
                printf("<label for=\"color\">Color to Search By:</label>\n");
                //fetch and display car names as datalist options
                printf("<select style=\"width: 500px;\" id=\"color\" name=\"color\" required>\n");

                sqlite3 *db = Connect();
                sqlite3_stmt *stmt;
                char sql[] = "SELECT Color FROM Car";
                sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    printf("<option value=\"%s\">%s</option>\n",
                           sqlite3_column_text(stmt, 0),
                           sqlite3_column_text(stmt, 0));
                }

                printf("</select>\n");

                printf("<br><input type=\"submit\" value=\"List Cars\">\n");
                printf("</form>\n");

                //close the database and finalize the statement
                sqlite3_finalize(stmt);
                sqlite3_close(db);
            }
            else if (strncmp(action, "ByMileage", 9) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ByMileage");
                if (car_data)
                {
                    car_data += 9; //skip "ByMileage="

                    int mileage = atoi(strstr(post_data_3, "mileage=") + 8);
                    char *direction = strstr(post_data_3, "direction=") + 10;

                    //additional handling for the direction
                    char *delimiter_pos = strchr(direction, '&'); //get where the & is
                    if (delimiter_pos != NULL)
                    {
                        // Copy the substring up to the delimiter
                        size_t length = delimiter_pos - direction; // Length of substring up to delimiter
                        strncpy(direction, direction, length);  // Copy the substring into the output
                        direction[length] = '\0';         // Null-terminate the output string
                    }
                    else
                    {
                        // If no delimiter is found, copy the whole string
                        strcpy(direction, direction);
                    }

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char *query;
                    if(strncmp(direction, "GT", strlen(direction)) == 0)
                    {
                        query = "SELECT * FROM Car WHERE Mileage > ?";
                    }
                    else
                    {
                        query = "SELECT * FROM Car WHERE Mileage < ?";
                    }
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the mileage
                    sqlite3_bind_int(stmt, 1, mileage);

                    //iterate through each step (and also get the names of the make and model while doing so!
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        char query[MAXLEN];
                        snprintf(query, sizeof(query),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("<p>%s %s %s %d, Valued at: $%.2f With: %.2fmi/Gal With:%d License Plate:%s VIN:%s</p>\n"
                                , sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1)
                                , sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 8)
                                , sqlite3_column_int(stmt, 6), sqlite3_column_text(stmt, 7), sqlite3_column_text(stmt, 9));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Cars Found</p>");
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Cars by Mileage</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ByMileage\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ByMileage\">\n");

                //year number
                printf("<label for=\"mileage\">Mileage to Search By:</label>\n");
                printf("<input type=\"number\"  name=\"mileage\"  id=\"mileage\" step=\"1\" value=\"50000\" required><br>\n");

                //less than or greater than (value number)
                printf("<p>Greater Than or Less Than the given value:</p>");
                printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
                printf("<label for=\"greater\" >Greater Than</label>");
                printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
                printf("<label for=\"lesser\">Less Than</label>");

                printf("<br><input type=\"submit\" value=\"List Cars\">\n");
                printf("</form>\n");
            }
            else if (strncmp(action, "ByMpg", 5) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ByMpg");
                if (car_data)
                {
                    car_data += 6;

                    double mpg = atof(strstr(post_data_3, "mpg=") + 4);
                    char *direction = strstr(post_data_3, "direction=") + 10;

                    //additional handling for the direction
                    char *delimiter_pos = strchr(direction, '&'); //get where the & is
                    if (delimiter_pos != NULL)
                    {
                        // Copy the substring up to the delimiter
                        size_t length = delimiter_pos - direction; // Length of substring up to delimiter
                        strncpy(direction, direction, length);  // Copy the substring into the output
                        direction[length] = '\0';         // Null-terminate the output string
                    }
                    else
                    {
                        // If no delimiter is found, copy the whole string
                        strcpy(direction, direction);
                    }

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char *query;
                    if(strncmp(direction, "GT", strlen(direction)) == 0)
                    {
                        query = "SELECT * FROM Car WHERE Miles_PerGal > ?";
                    }
                    else
                    {
                        query = "SELECT * FROM Car WHERE Miles_PerGal < ?";
                    }
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the mpg
                    sqlite3_bind_double(stmt, 1, mpg);

                    //iterate through each step (and also get the names of the make and model while doing so!
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        char query[MAXLEN];
                        snprintf(query, sizeof(query),
                                 "SELECT Make.Name, Model.Name FROM Make, Model WHERE Make.Id = %d AND Model.Id = %d",
                                 sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
                        sqlite3_stmt *stmt2;
                        sqlite3_prepare_v2(db, query, -1, &stmt2, NULL);
                        sqlite3_step(stmt2);

                        printf("<p>%s %s %s %d, Valued at: $%.2f With: %.2fmi/Gal With:%d License Plate:%s VIN:%s</p>\n"
                                , sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt2, 0), sqlite3_column_text(stmt2, 1)
                                , sqlite3_column_int(stmt, 2), sqlite3_column_double(stmt, 5), sqlite3_column_double(stmt, 8)
                                , sqlite3_column_int(stmt, 6), sqlite3_column_text(stmt, 7), sqlite3_column_text(stmt, 9));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Cars Found</p>");
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Cars by Mile Per Gallon</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ByMpg\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ByMpg\">\n");

                //year number
                printf("<label for=\"mileage\">Mi/Gal to Search By:</label>\n");
                printf("<input type=\"number\"  name=\"mpg\"  id=\"mpg\" step=\"1\" value=\"20.0\" required><br>\n");

                //less than or greater than (value number)
                printf("<p>Greater Than or Less Than the given value:</p>");
                printf("<input type=\"radio\" name=\"direction\" id=\"greater\" value=\"GT\" required>");
                printf("<label for=\"greater\" >Greater Than</label>");
                printf("<input type=\"radio\" name=\"direction\" id=\"lesser\" value=\"LT\" required>");
                printf("<label for=\"lesser\">Less Than</label>");

                printf("<br><input type=\"submit\" value=\"List Cars\">\n");
                printf("</form>\n");
            }
            else if (strncmp(action, "ModelByMake", 11) == 0)
            {
                printf("Content-type: text/html\n\n");

                //get the needed (2) pieces of information needed for the query
                char *car_data = strstr(post_data_2, "ModelByMake");
                if (car_data)
                {
                    car_data += 12;

                    int make_id = atoi(strstr(post_data_3, "make=") + 5);

                    //set up the proper query
                    sqlite3* db = Connect();
                    sqlite3_stmt *stmt;
                    char query[] = "SELECT Name FROM Model WHERE Make = ?";
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
                    //bind the make id
                    sqlite3_bind_int(stmt, 1, make_id);

                    //iterate through each step
                    int iterations = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        printf("<p>%s</p>\n"
                                , sqlite3_column_text(stmt, 0));
                        iterations++;
                    }
                    if(iterations == 0) printf("<p>No Models Found</p>");
                }
                else
                {
                    printf("<p>Error processing POST</p>");
                }

                // Generate the form
                printf("<h1 class=\"main_container\">List Model(s) by Make</h1>\n");
                printf("<form class=\"main_container\" action=\"/cgi-bin/HD_Corp.exe\" method=\"POST\">\n");
                // Hidden input to pass the page context
                printf("<input type=\"hidden\" name=\"page\" value=\"ModelByMake\">\n");
                printf("<input type=\"hidden\" name=\"action\" value=\"ModelByMake\">\n");

                //makes
                printf("<label for=\"make\">Make to Search By:</label>\n");
                //fetch and display car names as datalist options
                printf("<select style=\"width: 500px;\" id=\"make\" name=\"make\" required>\n");

                sqlite3 *db = Connect();
                sqlite3_stmt *stmt;
                char sql[] = "SELECT * FROM Make";
                sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

                while (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    printf("<option value=\"%d\">%s</option>\n",
                           sqlite3_column_int(stmt, 0),
                           sqlite3_column_text(stmt, 1));
                }

                printf("</select>\n");

                printf("<br><input type=\"submit\" value=\"List Models\">\n");
                printf("</form>\n");

                //close the database and finalize the statement
                sqlite3_finalize(stmt);
                sqlite3_close(db);
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
//function to read the post data
void read_post_data(char *data, int size)
{
    int content_length = atoi(getenv("CONTENT_LENGTH"));
    if (content_length > 0)
    {
        fgets(data, size, stdin);
    }
}

// Function to convert a hexadecimal string to a byte array
int hex_to_bytes(const char* hex_str, uint8_t* bytes)
{
    size_t len = strlen(hex_str);
    if (len % 2 != 0) return -1; // Invalid hex string length
    for (size_t i = 0; i < len / 2; i++)
    {
        sscanf(hex_str + 2 * i, "%2hhx", &bytes[i]);
    }
    return 0;
}

// Function to extract parameter from query string
char* get_query_param(const char* query_string, const char* param_name)
{
    if (!query_string || !param_name) return NULL;

    char *query_copy = strdup(query_string); // Make a copy to safely modify
    char *pair = strtok(query_copy, "&");

    while (pair != NULL)
    {
        char *equal_sign = strchr(pair, '=');
        if (equal_sign)
        {
            *equal_sign = '\0'; // Split into key and value
            char *key = pair;
            char *value = equal_sign + 1;

            if (strcmp(key, param_name) == 0)
            {
                // Copy value before freeing query_copy
                char *result = strdup(value);
                free(query_copy);
                return result;
            }
        }
        pair = strtok(NULL, "&");
    }

    free(query_copy);
    return NULL;
}

//function to remove white space from a string
void strip_spaces(char *str)
{
    char *end;

    //trim leading spaces
    while (isspace((unsigned char)*str)) str++;

    //trim trailing spaces
    if (*str == 0)  //all spaces
        return;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    //null-terminate the string
    *(end + 1) = 0;
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