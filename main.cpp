#include <iostream>
#include <string>
using namespace std;

int main() {
    cout << unitbuf;
    cerr << unitbuf;
    
    while(true) {
        cout << "$ ";
        string input;
        
        if(!getline(cin, input)) {
            break;
        }

        if (input == "\\q") {
            cout << "Exit" << endl;
            break;
        }

        if (input.find("echo ") == 0) {
            string message = input.substr(5);
            cout << message << endl;
        }
        else if (!input.empty()) {
            cout << "ERROR" << endl;
        }
    }
    
    return 0;
}
