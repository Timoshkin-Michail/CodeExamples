package com.mikhail.timoshkin.webshop;

public interface Commands {
    interface Server {
        String stop_server = "stop_server";
        String add_item = "add_item"; // name price count
    }

    interface Client {
        String register = "register";
        String exit = "exit";
        String add_to_cart = "add_to_cart";
        String rem_from_cart = "rem_from_cart";
        String buy = "buy";
        String discard = "discard";
        String show_all = "show_all";
        String list_categories = "list_categories";
        String show_category = "show_category";
        String statistics = "statistics";
    }
}
