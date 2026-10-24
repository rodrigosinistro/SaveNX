#include "remote/URL.hpp"
#include "remote/Form.hpp"
#include <cassert>
#include <string>
#include <iostream>

int main()
{
    remote::URL url("https://www.googleapis.com/drive/v3/files");
    url.append_parameter("q", "name = 'Pokémon & save'").append_parameter("pageToken", "a+b/c==");
    const std::string request = url.get();
    assert(request.find("q=name%20%3D%20%27Pok%C3%A9mon%20%26%20save%27") != request.npos);
    assert(request.find("pageToken=a%2Bb%2Fc%3D%3D") != request.npos);
    assert(request.find(' ') == request.npos);
    remote::Form form;
    form.append_parameter("client_secret", "a&b+c=").append_parameter("grant_type", "urn:test:device");
    assert(std::string(form.get()) == "client_secret=a%26b%2Bc%3D&grant_type=urn%3Atest%3Adevice");
    std::cout << "OAuth form and Drive query encoding: PASS\n";
}
